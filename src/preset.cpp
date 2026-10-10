#include "preset.hpp"
#include <bcrypt.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dlss {
namespace {
[[noreturn]] void Stop(const std::string& message) { throw std::runtime_error(message); }
std::string Key(const std::wstring& profile) {
    const auto text = Utf8(Lower(profile));
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) Stop(TrUtf8("Yedek anahtarı oluşturulamadı."));
    std::array<unsigned char, 32> digest{};
    const auto status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),
        ULONG(text.size()), digest.data(), ULONG(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) Stop(TrUtf8("Yedek anahtarı oluşturulamadı."));
    std::ostringstream result;
    for (auto byte : digest) result << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    return result.str();
}
std::string ExeKey(const Game& game) { return Utf8(Lower(fs::absolute(game.exe).lexically_normal().wstring())); }
void Scope(const Game& game, const DriverSnapshot& snapshot) {
    if (game.exe.empty() || !game.exe.is_absolute()) Stop(TrUtf8("Önce oyunun gerçek EXE dosyasını seçin."));
    if (snapshot.baseProfile || snapshot.profile.empty()) Stop(TrUtf8("Genel NVIDIA profili değiştirilemez. Oyuna özel profil gerekli."));
    const auto exe = Lower(game.exe.filename().wstring());
    bool found = false;
    for (const auto& application : snapshot.applications) {
        const auto name = Lower(fs::path(application).filename().wstring());
        if (name == exe) { found = true; continue; }
        // NVIDIA's Cyberpunk profile also contains its own launcher. No unrelated game may share the write.
        if (exe == L"cyberpunk2077.exe" && snapshot.profile == L"Cyberpunk 2077" && name == L"cplauncher.exe") continue;
        Stop(TrUtf8("NVIDIA profili başka bir programa da bağlı. Otomatik ayar bu profil için desteklenmiyor."));
    }
    if (!found) Stop(TrUtf8("NVIDIA profili seçili oyunla eşleşmiyor."));
}
bool Target(const DriverValue& value, size_t index) { return value.present && value.local && value.value == FgValues[index]; }
bool EffectiveTarget(const DriverSnapshot& snapshot) {
    for (size_t i = 0; i < FgIds.size(); ++i) if (!snapshot.settings[i].present || snapshot.settings[i].value != FgValues[i]) return false;
    return true;
}
json Encode(const DriverValue& value, size_t index) {
    return {{"id", FgIds[index]}, {"present", value.present}, {"local", value.local}, {"value", value.value}};
}
std::array<DriverValue, 2> ReadBackup(const fs::path& path, const Game& game, const DriverSnapshot& snapshot) {
    RejectLinkedPath(path);
    const auto doc = json::parse(ReadText(path, 16384));
    if (doc.value("app", "") != "dlssalltools" || doc.value("schema", 0) != 1 || doc.value("kind", "") != "nvidia-fg-preset-b" ||
        doc.value("profile", "") != Utf8(snapshot.profile) || doc.value("exe", "") != ExeKey(game) ||
        !doc.contains("settings") || !doc["settings"].is_array() || doc["settings"].size() != 2)
        Stop(TrUtf8("Ayar yedeği bu oyun/profille eşleşmiyor. NVIDIA ayarları değiştirilmedi."));
    std::array<DriverValue, 2> result;
    for (size_t i = 0; i < result.size(); ++i) {
        const auto& item = doc["settings"][i];
        if (item.at("id") != FgIds[i] || !item.at("local").is_boolean() || !item.at("present").is_boolean() ||
            !item.at("value").is_number_unsigned() || item.at("value").get<uint64_t>() > UINT32_MAX)
            Stop(TrUtf8("Ayar yedeği doğrulanamadı."));
        result[i] = {item.at("present").get<bool>(), item.at("local").get<bool>(), item.at("value").get<uint32_t>()};
        if (result[i].local && !result[i].present) Stop(TrUtf8("Ayar yedeği tutarsız."));
    }
    return result;
}
bool Restored(const DriverValue& current, const DriverValue& before) {
    // Inherited defaults may change after a driver update; restore inheritance, not a stale copied value.
    return before.local ? current == before : !current.local;
}
void Compatible(const DriverSnapshot& snapshot, const std::array<DriverValue, 2>& before) {
    for (size_t i = 0; i < before.size(); ++i)
        if (!Target(snapshot.settings[i], i) && !Restored(snapshot.settings[i], before[i]))
            Stop(TrUtf8("NVIDIA ayarı sonradan başka bir yerde değiştirilmiş. Üzerine yazmamak için işlem durduruldu."));
}
}
fs::path PresetBackupPath(const fs::path& state, const std::wstring& profile) { return state / L"profile-fixes" / (Wide(Key(profile)) + L".json"); }
PresetFixState InspectPresetFix(const Game& game, const fs::path& state, ProfileStore& driver) {
    const auto snapshot = driver.Read(); Scope(game, snapshot);
    PresetFixState result; result.profile = snapshot.profile;
    const auto backup = PresetBackupPath(state, snapshot.profile); RejectLinkedPath(backup);
    result.managed = fs::exists(backup);
    if (result.managed) {
        try {
            const auto before = ReadBackup(backup, game, snapshot); Compatible(snapshot, before);
            result.enabled = Target(snapshot.settings[0], 0) && Target(snapshot.settings[1], 1);
            result.message = result.enabled ? Tr(L"Preset B ve DLSS-FG override kayıtlı. Oyunu tamamen kapatıp yeniden açın.")
                : Tr(L"Yedek bulundu; işlem tamamlanmamış veya ayar değişmiş. Kutuyu kaldırarak önceki ayarı geri alın.");
        } catch (const std::exception& e) { result.conflict = true; result.message = Wide(e.what()); }
    } else if (EffectiveTarget(snapshot)) {
        result.alreadySet = true;
        result.message = Tr(L"Bu iki ayar zaten etkin. Uygulama bir değişiklik yapmadı; sorunun başka bir nedeni olabilir.");
    } else result.message = Tr(L"Hazır. Kutuyu işaretlediğinizde bu oyunun NVIDIA profiline uygulanır.");
    return result;
}
void SetPresetFix(const Game& game, const fs::path& state, ProfileStore& driver, bool enable) {
    if (game.exe.empty() || !game.exe.is_absolute()) Stop(TrUtf8("Oyunun gerçek EXE dosyasını seçin."));
    RejectLinkedPath(game.exe); RejectLinkedPath(state);
    if (GameRunning(game.exe)) Stop(TrUtf8("Önce oyunu tamamen kapatın. Sonra seçeneği yeniden deneyin."));
    if (enable && !IsX64Image(game.exe, false)) Stop(TrUtf8("Geçerli bir 64 bit oyun EXE dosyası gerekli."));
    driver.Reload(); const auto snapshot = driver.Read(); Scope(game, snapshot);
    const auto backup = PresetBackupPath(state, snapshot.profile); RejectLinkedPath(backup);
    if (enable) {
        if (fs::exists(backup)) Stop(TrUtf8("Bu profilin ayar yedeği zaten var. Önce kutuyu kaldırarak geri alın."));
        if (EffectiveTarget(snapshot)) Stop(TrUtf8("Preset B ve DLSS-FG override zaten etkin; değişiklik yapılmadı."));
        const auto doc = json{{"app", "dlssalltools"}, {"schema", 1}, {"kind", "nvidia-fg-preset-b"}, {"exe", ExeKey(game)},
            {"profile", Utf8(snapshot.profile)}, {"state", "pending"}, {"settings", json::array({Encode(snapshot.settings[0], 0), Encode(snapshot.settings[1], 1)})}};
        NewJson(backup, doc); // Never overwrite an earlier restore point; durable before the first driver write.
        try {
            driver.Stage(0, FgValues[0]); driver.Stage(1, FgValues[1]); driver.Save();
            driver.Reload(); const auto checked = driver.Read(); Scope(game, checked);
            if (checked.profile != snapshot.profile || !Target(checked.settings[0], 0) || !Target(checked.settings[1], 1))
                Stop(TrUtf8("NVIDIA ayarları kaydedildikten sonra doğrulanamadı."));
            auto complete = doc; complete["state"] = "active"; AtomicJson(backup, complete);
        } catch (...) { try { driver.Reload(); } catch (...) {} throw; }
    } else {
        if (!fs::exists(backup)) Stop(TrUtf8("Bu uygulamaya ait ayar yedeği bulunamadı. Mevcut ayarlara dokunulmadı."));
        const auto before = ReadBackup(backup, game, snapshot); Compatible(snapshot, before);
        try {
            for (size_t i = 0; i < before.size(); ++i) if (!Restored(snapshot.settings[i], before[i]))
                driver.Stage(i, before[i].local ? std::optional<uint32_t>(before[i].value) : std::nullopt);
            driver.Save(); driver.Reload(); const auto checked = driver.Read(); Scope(game, checked);
            if (checked.profile != snapshot.profile) Stop(TrUtf8("NVIDIA profili işlem sırasında değişti; yedek korundu."));
            for (size_t i = 0; i < before.size(); ++i) if (!Restored(checked.settings[i], before[i])) Stop(TrUtf8("Önceki NVIDIA ayarı doğrulanamadı; yedek korundu."));
            if (!fs::remove(backup)) Stop(TrUtf8("Önceki ayarlar geri alındı; yedek kaydı temizlenemedi."));
        } catch (...) { try { driver.Reload(); } catch (...) {} throw; }
    }
}
}
