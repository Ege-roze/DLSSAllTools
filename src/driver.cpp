#include "preset.hpp"
#include "../third_party/nvapi/nvapi.h"
#include "../third_party/nvapi/NvApiDriverSettings.h"
#include <algorithm>
#include <stdexcept>

namespace dlss {
namespace {
static_assert(FgEnableId == NGX_DLSS_FG_OVERRIDE_ID && FgPresetId == NGX_DLSS_FG_OVERRIDE_RENDER_PRESET_SELECTION_ID);
static_assert(FgValues[0] == NGX_DLSS_FG_OVERRIDE_ON && FgValues[1] == NGX_DLSS_FG_OVERRIDE_RENDER_PRESET_SELECTION_RENDER_PRESET_B);
class NvProfile final : public ProfileStore {
    HMODULE module_ = nullptr;
    using Query = void* (__cdecl*)(unsigned int);
    Query query_ = nullptr;
    NvDRSSessionHandle session_ = nullptr;
    NvDRSProfileHandle profile_ = nullptr;
    fs::path exe_;
    bool initialized_ = false;
    template<class F> F Entry(uint32_t id) {
        auto address = query_(id);
        if (!address) throw std::runtime_error(TrUtf8("NVIDIA sürücüsü gerekli profil işlevini desteklemiyor."));
        return reinterpret_cast<F>(address);
    }
    void Check(NvAPI_Status code) {
        if (code == NVAPI_OK) return;
        if (code == NVAPI_INVALID_USER_PRIVILEGE || code == NVAPI_ACCESS_DENIED)
            throw std::runtime_error(TrUtf8("NVIDIA ayarı için uygulamayı yönetici olarak açıp yeniden deneyin."));
        if (code == NVAPI_EXECUTABLE_NOT_FOUND)
            throw std::runtime_error(TrUtf8("Bu oyun için NVIDIA profili bulunamadı. Genel profil değiştirilmedi."));
        NvAPI_ShortString detail{};
        Entry<decltype(&NvAPI_GetErrorMessage)>(0x6c2d048c)(code, detail);
        throw std::runtime_error(TrUtf8("NVIDIA profil işlemi tamamlanamadı: ") + std::string(detail) + " (" + std::to_string(code) + ")");
    }
    void Resolve() {
        NvAPI_UnicodeString name{};
        const auto text = exe_.wstring();
        if (text.size() >= std::size(name)) throw std::runtime_error(TrUtf8("Oyun yolu NVIDIA profil sınırını aşıyor."));
        std::copy(text.begin(), text.end(), name);
        NVDRS_APPLICATION application{}; application.version = NVDRS_APPLICATION_VER;
        Check(Entry<decltype(&NvAPI_DRS_FindApplicationByName)>(0xeee566b2)(session_, name, &profile_, &application));
    }
    void Close() noexcept {
        if (session_ && query_) {
            if (auto f = reinterpret_cast<decltype(&NvAPI_DRS_DestroySession)>(query_(0xdad9cff8))) f(session_);
        }
        if (initialized_ && query_) {
            if (auto f = reinterpret_cast<decltype(&NvAPI_Unload)>(query_(0xd22bdd7e))) f();
        }
        if (module_) FreeLibrary(module_);
    }
public:
    explicit NvProfile(const Game& game) : exe_(game.exe) {
        try {
            if (exe_.empty() || !exe_.is_absolute()) throw std::runtime_error(TrUtf8("Önce oyunun gerçek EXE dosyasını seçin."));
            // Load only the installed Windows driver, never a DLL next to the game/app.
            module_ = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module_) throw std::runtime_error(TrUtf8("NVIDIA sürücü arayüzü bulunamadı."));
            query_ = reinterpret_cast<Query>(GetProcAddress(module_, "nvapi_QueryInterface"));
            if (!query_) throw std::runtime_error(TrUtf8("NVIDIA profil arayüzü kullanılamıyor."));
            Check(Entry<decltype(&NvAPI_Initialize)>(0x0150e828)()); initialized_ = true;
            Check(Entry<decltype(&NvAPI_DRS_CreateSession)>(0x0694d52e)(&session_));
            Reload();
            for (auto id : FgIds) {
                NvAPI_UnicodeString name{};
                Check(Entry<decltype(&NvAPI_DRS_GetSettingNameFromId)>(0xd61cbe6e)(id, &name));
            }
        } catch (...) { Close(); throw; }
    }
    ~NvProfile() override { Close(); }
    void Reload() override {
        Check(Entry<decltype(&NvAPI_DRS_LoadSettings)>(0x375dbd6b)(session_)); Resolve();
    }
    DriverSnapshot Read() override {
        DriverSnapshot snapshot;
        NvDRSProfileHandle base = nullptr;
        Check(Entry<decltype(&NvAPI_DRS_GetBaseProfile)>(0xda8466a0)(session_, &base));
        snapshot.baseProfile = base == profile_;
        NVDRS_PROFILE info{}; info.version = NVDRS_PROFILE_VER;
        Check(Entry<decltype(&NvAPI_DRS_GetProfileInfo)>(0x61cd6fd6)(session_, profile_, &info));
        snapshot.profile = reinterpret_cast<const wchar_t*>(info.profileName);
        if (info.numOfApps > 256) throw std::runtime_error(TrUtf8("NVIDIA profili çok sayıda uygulamaya bağlı; otomatik değişiklik kapalı."));
        if (info.numOfApps) {
            std::vector<NVDRS_APPLICATION> apps(info.numOfApps);
            for (auto& app : apps) app.version = NVDRS_APPLICATION_VER;
            NvU32 count = info.numOfApps;
            Check(Entry<decltype(&NvAPI_DRS_EnumApplications)>(0x7fa2173a)(session_, profile_, 0, &count, apps.data()));
            if (count > apps.size()) throw std::runtime_error(TrUtf8("NVIDIA profil uygulamaları okunamadı."));
            for (size_t i = 0; i < count; ++i) snapshot.applications.emplace_back(reinterpret_cast<const wchar_t*>(apps[i].appName));
        }
        for (size_t i = 0; i < FgIds.size(); ++i) {
            NVDRS_SETTING setting{}; setting.version = NVDRS_SETTING_VER;
            auto code = Entry<decltype(&NvAPI_DRS_GetSetting)>(0x73bf8338)(session_, profile_, FgIds[i], &setting);
            if (code == NVAPI_SETTING_NOT_FOUND) continue;
            Check(code);
            if (setting.settingType != NVDRS_DWORD_TYPE) throw std::runtime_error(TrUtf8("NVIDIA ayar türü beklenenden farklı; işlem durduruldu."));
            snapshot.settings[i] = {true, setting.settingLocation == NVDRS_CURRENT_PROFILE_LOCATION && !setting.isCurrentPredefined,
                setting.u32CurrentValue};
        }
        return snapshot;
    }
    void Stage(size_t index, std::optional<uint32_t> value) override {
        if (index >= FgIds.size()) throw std::runtime_error(TrUtf8("İzin verilmeyen NVIDIA ayarı."));
        if (Read().baseProfile) throw std::runtime_error(TrUtf8("Genel NVIDIA profili değiştirilemez."));
        if (value) {
            NVDRS_SETTING setting{}; setting.version = NVDRS_SETTING_VER;
            setting.settingId = FgIds[index]; setting.settingType = NVDRS_DWORD_TYPE; setting.u32CurrentValue = *value;
            Check(Entry<decltype(&NvAPI_DRS_SetSetting)>(0x577dd202)(session_, profile_, &setting));
        } else {
            auto code = Entry<decltype(&NvAPI_DRS_DeleteProfileSetting)>(0xe4a26362)(session_, profile_, FgIds[index]);
            if (code != NVAPI_SETTING_NOT_FOUND) Check(code);
        }
    }
    void Save() override { Check(Entry<decltype(&NvAPI_DRS_SaveSettings)>(0xfcbc7e14)(session_)); }
};
}
std::unique_ptr<ProfileStore> OpenDriverProfile(const Game& game) { return std::make_unique<NvProfile>(game); }
json DriverReport(const Game& game) {
    auto driver = OpenDriverProfile(game); const auto snapshot = driver->Read();
    json apps = json::array(), settings = json::array();
    for (const auto& app : snapshot.applications) apps.push_back(Utf8(app));
    for (size_t i = 0; i < FgIds.size(); ++i) settings.push_back({{"id", FgIds[i]}, {"present", snapshot.settings[i].present},
        {"local", snapshot.settings[i].local}, {"value", snapshot.settings[i].value}});
    return {{"profile", Utf8(snapshot.profile)}, {"baseProfile", snapshot.baseProfile}, {"applications", apps}, {"settings", settings}};
}
}
