#include "neural.hpp"
#include "neural_catalog.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace dlss {
namespace {
[[noreturn]] void Error(const std::string& text) { throw std::runtime_error(text); }
const json& Catalog() { static const auto data = json::parse(NeuralCatalogJson); return data; }
const wchar_t* JournalDir(FileSet kind=FileSet::Neural) { return kind==FileSet::Neural ? L".dlssalltools-dlss5" : L".dlssalltools-mfg-sm86"; }
const char* JournalApp(FileSet kind) { return kind==FileSet::Neural ? "dlssalltools-neural" : "dlssalltools-mfg-sm86"; }
std::string Normal(const fs::path& path) { return Utf8(Lower(fs::absolute(path).lexically_normal().wstring())); }
bool HashValid(const std::string& value) { return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == value.npos; }
bool ConfigPath(const std::wstring& path, FileSet kind=FileSet::Neural) { if(kind==FileSet::Sm86) return path==L"dlssg_sm86.ini"; return path == L"ReShade.ini" || path == L"DLSSAllTools-DLSS5.ini" || path == L"dlss5-feed.cfg"; }
bool Allowed(const std::wstring& path, FileSet kind) {
    if(kind==FileSet::Sm86) return path==L"version.dll" || path==L"winmm.dll" || path==L"dbghelp.dll" || path==L"dinput8.dll" || path==L"dlssg_sm86.ini";
    if (ConfigPath(path,kind)) return true;
    for (const auto& c : Catalog()) for (const auto& f : c.at("files")) if (Wide(f.at("path").get<std::string>()) == path) return true;
    return false;
}
fs::path Target(const Game& game, const std::wstring& relative, FileSet kind=FileSet::Neural) {
    const fs::path part(relative);
    if (!Allowed(relative,kind) || part.is_absolute() || part.has_root_name() || relative.find(L':') != relative.npos) Error(TrUtf8("DLSS 5 kaydında geçersiz dosya yolu."));
    for (const auto& element : part) if (element == L".." || element == L".") Error(TrUtf8("DLSS 5 dosyası oyun klasörü dışına çıkamaz."));
    auto path = game.exe.parent_path() / part; RejectLinkedPath(path); return path;
}
void CheckGame(const Game& game, bool installing) {
    if (game.exe.empty() || !game.exe.is_absolute() || !fs::is_directory(game.exe.parent_path())) Error(TrUtf8("Gerçek oyun EXE dosyasını seçin."));
    RejectLinkedPath(game.exe);
    if (GameRunning(game.exe)) Error(TrUtf8("Önce oyunu tamamen kapatın."));
    if (installing && !IsX64Image(game.exe, false)) Error(TrUtf8("DLSS 5 kurulumu için 64 bit oyun EXE dosyası gerekli."));
}
void CopyChecked(const fs::path& source, const fs::path& dest, const std::string& hash, bool replace) {
    RejectLinkedPath(source); RejectLinkedPath(dest); fs::create_directories(dest.parent_path());
    auto temp = dest; temp += L".dlssalltools-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    if (!CopyFileW(source.c_str(), temp.c_str(), TRUE)) Error(TrUtf8("Dosya kopyalanamadı; yazma iznini ve boş disk alanını kontrol edin."));
    try {
        if (Sha256(temp) != hash) Error(TrUtf8("Kopyalanan dosya doğrulanamadı."));
        HANDLE handle = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) Error(TrUtf8("Dosya diske kaydedilemedi."));
        const bool flushed = FlushFileBuffers(handle) != FALSE; CloseHandle(handle);
        if (!flushed) Error(TrUtf8("Dosya diske kaydedilemedi."));
        RejectLinkedPath(dest);
        if (!MoveFileExW(temp.c_str(), dest.c_str(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) Error(TrUtf8("Dosya kullanımda veya yazma izni yok. İşlemi yeniden deneyin."));
    } catch (...) { std::error_code ec; fs::remove(temp, ec); throw; }
}
void WriteNewText(const fs::path& file, const std::string& text) {
    RejectLinkedPath(file); fs::create_directories(file.parent_path());
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) Error(TrUtf8("Ayar dosyası oluşturulamadı."));
    DWORD written = 0; const bool ok = WriteFile(h, text.data(), DWORD(text.size()), &written, nullptr) && written == text.size() && FlushFileBuffers(h);
    CloseHandle(h); if (!ok) Error(TrUtf8("Ayar dosyası kaydedilemedi."));
}
json ReadJournal(const Game& game, FileSet kind=FileSet::Neural) {
    const auto dir = game.exe.parent_path() / JournalDir(kind); RejectLinkedPath(dir);
    const auto j = json::parse(ReadText(dir / L"manifest.json"));
    if (j.at("app").get<std::string>() != JournalApp(kind) || j.at("schema") != 1 || j.at("exe").get<std::string>() != Normal(game.exe)) Error(TrUtf8("DLSS 5 kurulum kaydı bu oyuna ait değil."));
    const std::string phase = j.at("state").get<std::string>();
    if (phase != "preparing" && phase != "installing" && phase != "installed" && phase != "restoring" && phase != "restored") Error(TrUtf8("DLSS 5 kurulum kaydı geçersiz."));
    if (!j.at("files").is_array() || j.at("files").empty() || j.at("files").size() > 80) Error(TrUtf8("DLSS 5 dosya listesi geçersiz."));
    std::set<std::wstring> names;
    for (const auto& f : j.at("files")) {
        const auto relative = Wide(f.at("path").get<std::string>()); Target(game, relative,kind);
        if (!names.insert(Lower(relative)).second || !HashValid(f.at("hash").get<std::string>())) Error(TrUtf8("DLSS 5 dosya kaydı geçersiz."));
        const std::string before = f.at("before").get<std::string>();
        if (!before.empty() && !HashValid(before)) Error(TrUtf8("DLSS 5 yedek kaydı geçersiz."));
    }
    return j;
}
bool Applicable(const json& component, NeuralGpu gpu) {
    if(!IsSupportedGpu(gpu)) return false;
    const auto code=component.at("gpu").get<std::string>();
    return code=="all" || code==GpuCode(gpu) || (code=="20,30" && IsOlderGpu(gpu));
}
bool ComponentReady(const fs::path& cache, const json& component) {
    try {
        for (const auto& f : component.at("files")) {
            const auto source = cache / Wide(component.at("id").get<std::string>()) / Wide(f.at("path").get<std::string>());
            RejectLinkedPath(source);
            if (!fs::is_regular_file(source) || Sha256(source) != f.at("sha256").get<std::string>()) return false;
        }
        return true;
    } catch (...) { return false; }
}
std::string TextOrEmpty(const fs::path& path) {
    if (!fs::exists(path)) return {};
    RejectLinkedPath(path); const auto text = ReadText(path);
    // Do not silently corrupt an existing UTF-16 configuration.
    if (text.size() >= 2 && static_cast<unsigned char>(text[0]) == 255 && static_cast<unsigned char>(text[1]) == 254) Error(TrUtf8("ReShade ayarı UTF-16 biçiminde. Mevcut ayar korunarak otomatik işlem durduruldu."));
    return text.starts_with("\xef\xbb\xbf") ? text.substr(3) : text;
}
std::string IniGet(const std::string& text, const std::string& section, const std::string& key) {
    std::istringstream in(text); std::string line, current;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto begin = line.find_first_not_of(" \t"), end = line.find_last_not_of(" \t");
        if (begin == line.npos) continue; line = line.substr(begin, end - begin + 1);
        if (line.starts_with('[') && line.ends_with(']')) { current = line.substr(1, line.size() - 2); continue; }
        const auto split = line.find('=');
        if (current == section && split != line.npos && line.substr(0, split) == key) return line.substr(split + 1);
    }
    return {};
}
bool DirectXGame(const Game& game) {
    const auto imports = PeImports(game.exe);
    for (const auto& name : imports) if (name == "d3d11.dll" || name == "d3d12.dll" || name == "dxgi.dll") return true;
    for (const auto& name : imports) if (name == "vulkan-1.dll" || name == "opengl32.dll" || name == "d3d9.dll") return false;
    // Common native-DLSS launchers resolve graphics exports dynamically.
    for (const auto* name : {L"nvngx_dlss.dll", L"sl.interposer.dll", L"D3D12Core.dll"}) if (fs::exists(game.exe.parent_path() / name)) return true;
    return false;
}
bool ReShadeAddon(const fs::path& path) {
    try { return IsX64Image(path, true) && ReadText(path, 64 * 1024 * 1024).find("Searching for add-ons") != std::string::npos; }
    catch (...) { return false; }
}
uint64_t FileVersionNumber(const fs::path& path) {
    DWORD unused = 0; const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &unused);
    if (!size || size > 1024 * 1024) return 0;
    std::vector<char> bytes(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) return 0;
    VS_FIXEDFILEINFO* info = nullptr; UINT length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&info), &length) || length < sizeof(*info)) return 0;
    return (uint64_t(info->dwFileVersionMS) << 32) | info->dwFileVersionLS;
}
}

std::string NeuralIniSet(const std::string& text, const std::string& section, const std::string& key, const std::string& value) {
    std::istringstream stream(text); std::vector<std::string> lines; std::string line;
    while (std::getline(stream, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); lines.push_back(line); }
    size_t start = lines.size(), end = lines.size();
    if (section.empty()) start = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto first = lines[i].find_first_not_of(" \t"), last = lines[i].find_last_not_of(" \t");
        const auto trimmed = first == std::string::npos ? "" : lines[i].substr(first, last - first + 1);
        if (trimmed == "[" + section + "]") { start = i + 1; end = lines.size(); }
        else if (start != lines.size() && i >= start && trimmed.starts_with('[')) { end = i; break; }
    }
    bool updated = false;
    if (start == lines.size() && !section.empty()) { lines.push_back("[" + section + "]"); end = lines.size(); }
    else for (size_t i = start; i < end; ++i) {
        const auto equal = lines[i].find('='); if (equal == lines[i].npos) continue;
        auto name = lines[i].substr(0, equal); const auto a = name.find_first_not_of(" \t"), b = name.find_last_not_of(" \t");
        if (a != name.npos && name.substr(a, b - a + 1) == key) { lines[i] = key + "=" + value; updated = true; }
    }
    if (!updated) lines.insert(lines.begin() + end, key + "=" + value);
    std::string out; for (const auto& l : lines) out += l + "\r\n"; return out;
}

void InstallNeuralFiles(const Game& game, const std::vector<NeuralFile>& files, const json& metadata, const Progress& progress, FileSet kind) {
    CheckGame(game, true);
    const auto journal = game.exe.parent_path() / JournalDir(kind); RejectLinkedPath(journal);
    if (fs::exists(journal)) Error(TrUtf8("Önce mevcut Bileşen kurulumunu Kaldır / Geri al ile temizleyin."));
    json entries = json::array(); std::set<std::wstring> names;
    for (const auto& f : files) {
        const auto dest = Target(game, f.destination,kind); RejectLinkedPath(f.source);
        if (!names.insert(Lower(f.destination)).second || !HashValid(f.hash) || Sha256(f.source) != f.hash) Error(TrUtf8("Bileşen kaynak dosyası doğrulanamadı."));
        const std::string before = fs::exists(dest) ? Sha256(dest) : "";
        if (before == f.hash) continue; // A pre-existing identical file is not ours to delete.
        entries.push_back({{"path", Utf8(f.destination)}, {"hash", f.hash}, {"before", before}, {"source", Utf8(f.source.wstring())}});
    }
    if (entries.empty()) Error(TrUtf8("Bileşenler zaten mevcut; bu uygulama tarafından kurulmadıkları için sahiplenilmedi."));
    if (!fs::create_directory(journal)) Error(TrUtf8("Bileşen yedek klasörü oluşturulamadı."));
    json doc{{"app", JournalApp(kind)}, {"schema", 1}, {"exe", Normal(game.exe)}, {"state", "preparing"}, {"files", entries}, {"options", metadata}};
    NewJson(journal / L"manifest.json", doc);
    // No game file is changed until every original and replacement is durable.
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& f = entries[i]; const auto dest = Target(game, Wide(f.at("path").get<std::string>()),kind);
        const auto number = std::to_wstring(i);
        if (f.at("before").get<std::string>() != "") CopyChecked(dest, journal / (number + L".before"), f.at("before").get<std::string>(), false);
        CopyChecked(fs::path(Wide(f.at("source").get<std::string>())), journal / (number + L".new"), f.at("hash").get<std::string>(), false);
    }
    doc["state"] = "installing"; AtomicJson(journal / L"manifest.json", doc);
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& f = entries[i]; const auto dest = Target(game, Wide(f.at("path").get<std::string>()),kind);
        if ((fs::exists(dest) ? Sha256(dest) : "") != f.at("before").get<std::string>()) Error(TrUtf8("Kurulum sırasında oyun dosyası değişti. Kaldır / Geri al ile önceki kuruluma dönün."));
        progress(Tr(L"Bileşen kuruluyor: ") + Wide(f.at("path").get<std::string>()));
        CopyChecked(journal / (std::to_wstring(i) + L".new"), dest, f.at("hash").get<std::string>(), f.at("before").get<std::string>() != "");
    }
    doc["state"] = "installed"; AtomicJson(journal / L"manifest.json", doc);
}

void RestoreNeuralFiles(const Game& game, const Progress& progress, FileSet kind) {
    CheckGame(game, false);
    const auto journal = game.exe.parent_path() / JournalDir(kind);
    auto doc = ReadJournal(game,kind); const auto& entries = doc.at("files");
    // Check the complete transaction BEFORE restoring any file.
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& f = entries[i]; const auto relative = Wide(f.at("path").get<std::string>()); const auto dest = Target(game, relative,kind);
        const std::string current = fs::exists(dest) ? Sha256(dest) : "", before = f.at("before").get<std::string>();
        if (current == before) continue;
        if (doc.at("state").get<std::string>() == "preparing") Error(TrUtf8("Yedekleme sırasında dosyalar değişmiş; otomatik geri alma durduruldu."));
        if (!current.empty() && current != f.at("hash").get<std::string>() && !ConfigPath(relative,kind)) Error(TrUtf8("Bileşen dosyası sonradan değişmiş: ") + Utf8(relative) + TrUtf8(". Dosya korunuyor; geri alma durduruldu."));
        if (!before.empty()) {
            const auto backup = journal / (std::to_wstring(i) + L".before"); RejectLinkedPath(backup);
            if (Sha256(backup) != before) Error(TrUtf8("Orijinal dosya yedeği doğrulanamadı. Geri alma durduruldu."));
        }
    }
    const bool preparing = doc.at("state").get<std::string>() == "preparing";
    doc["state"] = "restoring"; AtomicJson(journal / L"manifest.json", doc);
    if (!preparing) for (size_t index = entries.size(); index > 0; --index) {
        const size_t i = index - 1; const auto& f = entries[i];
        const auto relative = Wide(f.at("path").get<std::string>()); const auto dest = Target(game, relative,kind);
        const std::string current = fs::exists(dest) ? Sha256(dest) : "", before = f.at("before").get<std::string>();
        if (current == before) continue;
        if (!current.empty() && current != f.at("hash").get<std::string>() && !ConfigPath(relative,kind)) Error(TrUtf8("Geri alma sırasında dosya değişti."));
        progress(Tr(L"Önceki dosya geri alınıyor: ") + relative);
        if (ConfigPath(relative,kind) && !current.empty()) {
            // ReShade rewrites its INI during play. Preserve those later settings too.
            const auto saved = journal / L"son-ayarlar" / Wide(current + ".txt");
            if (!fs::exists(saved)) CopyChecked(dest, saved, current, false);
        }
        if (before.empty()) { if (!current.empty()) RemoveVerifiedFile(dest, current); }
        else CopyChecked(journal / (std::to_wstring(i) + L".before"), dest, before, !current.empty());
    }
    // Keep the archive, including post-play settings. Never recursively delete user directories.
    doc["state"] = "restored"; AtomicJson(journal / L"manifest.json", doc);
    auto archive = game.exe.parent_path() / (std::wstring(JournalDir(kind))+L"-restored-" + std::to_wstring(GetTickCount64()));
    if (!MoveFileExW(journal.c_str(), archive.c_str(), MOVEFILE_WRITE_THROUGH)) {
        doc["state"] = "restoring"; AtomicJson(journal / L"manifest.json", doc); Error(TrUtf8("Dosyalar geri alındı ancak yedek arşivlenemedi. Yeniden Kaldır seçin."));
    }
}

fs::path NeuralManager::Cache() const { return state_ / L"packages" / L"dlss5-2026-10-09"; }
bool NeuralManager::PackageReady(NeuralGpu gpu) const {
    if(!IsSupportedGpu(gpu)) return false;
    for (const auto& c : Catalog()) if (Applicable(c, gpu) && !ComponentReady(Cache(), c)) return false;
    return true;
}
void NeuralManager::Prepare(NeuralGpu gpu, const Progress& progress) const {
    GpuCode(gpu);
    RejectLinkedPath(Cache()); fs::create_directories(Cache());
    for (const auto& c : Catalog()) {
        if (!Applicable(c, gpu) || ComponentReady(Cache(), c)) continue;
        progress(Tr(L"DLSS 5 bileşeni hazırlanıyor: ") + Wide(c.at("id").get<std::string>()));
        const auto archive = Cache() / L"downloads" / Wide(c.at("archive").get<std::string>());
        DownloadVerified(Wide(c.at("url").get<std::string>()), archive, c.at("sha256").get<std::string>(), progress);
        const auto output = Cache() / Wide(c.at("id").get<std::string>()); RejectLinkedPath(output);
        const auto scratch = Cache() / (L"extract-" + Wide(c.at("id").get<std::string>()) + L"-" + std::to_wstring(GetTickCount64()));
        std::vector<std::wstring> entries;
        for (const auto& f : c.at("files")) if (f.at("entry").get<std::string>() != "") entries.push_back(Wide(f.at("entry").get<std::string>()));
        if (!entries.empty()) ExtractPinned(archive, scratch, entries);
        for (const auto& f : c.at("files")) {
            const auto src = f.at("entry").get<std::string>() == "" ? archive : scratch / Wide(f.at("entry").get<std::string>());
            CopyChecked(src, output / Wide(f.at("path").get<std::string>()), f.at("sha256").get<std::string>(), true);
        }
        // Only our newly created, pinned extraction tree is removed.
        RejectLinkedPath(scratch); if (fs::exists(scratch)) fs::remove_all(scratch);
    }
    if (!PackageReady(gpu)) Error(TrUtf8("DLSS 5 bileşen doğrulaması tamamlanamadı."));
    progress(Tr(L"DLSS 5 paketi hazır. Tüm bileşenler doğrulandı."));
}
NeuralState InspectManagedFiles(const Game& game, FileSet kind) {
    NeuralState state;
    if(game.exe.empty()) return state;
    try {
        RejectLinkedPath(game.exe);
        const auto journal=game.exe.parent_path()/JournalDir(kind); RejectLinkedPath(journal);
        if(!fs::exists(journal)) return state;
        const auto j=ReadJournal(game,kind); bool complete=j.at("state").get<std::string>()=="installed";
        for(const auto& f:j.at("files")) {
            const auto relative=Wide(f.at("path").get<std::string>()); const auto dest=Target(game,relative,kind);
            const std::string hash=fs::exists(dest)?Sha256(dest):"";
            if(hash!=f.at("hash").get<std::string>() && !ConfigPath(relative,kind)) complete=false;
            if(!hash.empty() && hash!=f.at("hash").get<std::string>() && hash!=f.at("before").get<std::string>() && !ConfigPath(relative,kind)) state.conflict=true;
        }
        state.installed=complete; state.recoverable=!complete;
        const auto label=kind==FileSet::Neural ? L"DLSS 5" : L"MFG 0.3.5";
        state.message=state.conflict ? Tr(L"Dosyalar sonradan değişmiş. Otomatik işlem durduruldu.") : complete ? std::wstring(label)+Tr(L" kurulu · RTX ")+Wide(j.at("options").value("gpu",std::string("?"))) : Tr(L"Yarım / eksik kurulum. Kaldır / Geri al ile kurtarabilirsiniz.");
    } catch(const std::exception& e) {state.conflict=true; state.message=Wide(e.what());}
    return state;
}
NeuralState NeuralManager::Inspect(const Game& game) const {
    auto state=InspectManagedFiles(game,FileSet::Neural);
    if(state.installed || state.recoverable || state.conflict) return state;
    if(game.exe.empty()) {state.message=Tr(L"Önce oyun EXE dosyasını seçin.");return state;}
    try {
        state.ready=fs::is_regular_file(game.exe) && IsX64Image(game.exe,false) && DirectXGame(game);
        state.message=state.ready ? Tr(L"64 bit DirectX 11/12 kurulumu hazır.") : Tr(L"Bu sürüm 64 bit DirectX 11/12 oyunlarını destekliyor. Gerçek oyun EXE dosyasını seçin.");
    } catch(const std::exception& e) {state.conflict=true;state.message=Wide(e.what());}
    return state;
}
void NeuralManager::Install(const Game& game, const NeuralOptions& options, const Progress& progress) const {
    GpuCode(options.gpu);
    CheckGame(game, true);
    const auto inspected = Inspect(game);
    if (!inspected.ready || inspected.conflict || inspected.installed || inspected.recoverable) Error(Utf8(inspected.message));
    const auto dir = game.exe.parent_path();
    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto name = Lower(entry.path().filename().wstring());
        if ((name.find(L"renodx-dlss") != name.npos || name.find(L"deep-fried-chicken") != name.npos || name == L"dlss5-feed.addon64") && name.ends_with(L".addon64"))
            Error(TrUtf8("Başka bir DLSS 5 eklentisi mevcut. Önce onu kendi kurulum aracıyla kaldırın."));
        if (name == L"reshade.asi") Error(TrUtf8("ASI üzerinden ReShade mevcut. İkinci ReShade kurulumu yapılmadı."));
    }
    const auto proxy = dir / L"dxgi.dll";
    bool reuse = fs::exists(proxy) && ReShadeAddon(proxy);
    if (fs::exists(proxy) && !reuse) {
        const auto hash = Sha256(proxy);
        for (const auto& r : Releases) if (hash == r.dllHash) Error(TrUtf8("MFG şu anda dxgi.dll kullanıyor. DLSS 5 için bu dosya değiştirilemez; önce MFG'nin ayrı yükleme yolu gerekir."));
        if (!options.replaceProxy) Error(TrUtf8("Mevcut dxgi.dll doğrulanmış bir ReShade Add-on sürümü değil. Değiştirmek istiyorsanız 'Mevcut grafik yükleyicisini yedekleyip değiştir' kutusunu işaretleyin."));
    }
    for (const auto* name : {L"d3d11.dll", L"d3d12.dll", L"opengl32.dll"})
        if (fs::exists(dir / name) && ReShadeAddon(dir / name)) Error(TrUtf8("Başka adla yüklenen ReShade bulundu. İkinci kopya kurulmadı; mevcut kurulumu önce kaldırın."));
    Prepare(options.gpu, progress);
    std::vector<NeuralFile> files;
    for (const auto& c : Catalog()) {
        if (!Applicable(c, options.gpu) || (c.at("feeder") == true && options.route != NeuralRoute::Feeder)) continue;
        for (const auto& f : c.at("files")) {
            const auto rel = Wide(f.at("path").get<std::string>());
            if (rel == L"dxgi.dll" && reuse) continue;
            const auto source = Cache() / Wide(c.at("id").get<std::string>()) / rel;
            // Newer existing SR DLLs remain the game's responsibility; do not downgrade them.
            if (rel == L"nvngx_dlss.dll" && fs::exists(dir / rel) && FileVersionNumber(dir / rel) > FileVersionNumber(source)) continue;
            files.push_back({source, rel, f.at("sha256").get<std::string>(), false});
        }
    }
    const auto configDir = Cache() / (L"config-" + std::to_wstring(GetTickCount64())); RejectLinkedPath(configDir); fs::create_directories(configDir);
    auto config = TextOrEmpty(Target(game, L"ReShade.ini"));
    config = NeuralIniSet(config, "ADDON", "AddonPath", ".\\");
    // An old disabled-addon entry must not leave a successful installation inert.
    if (IniGet(config, "ADDON", "DisabledAddons").find("RenoDX") != std::string::npos) Error(TrUtf8("ReShade ayarında RenoDX devre dışı. ReShade menüsünden etkinleştirip tekrar deneyin."));
    if (options.route == NeuralRoute::Feeder) {
        config = NeuralIniSet(config, "GENERAL", "EffectSearchPaths", ".\\DLSSAllTools-shaders\\Shaders\\**," + IniGet(config, "GENERAL", "EffectSearchPaths"));
        config = NeuralIniSet(config, "GENERAL", "TextureSearchPaths", ".\\DLSSAllTools-shaders\\Textures\\**," + IniGet(config, "GENERAL", "TextureSearchPaths"));
        config = NeuralIniSet(config, "GENERAL", "PresetPath", ".\\DLSSAllTools-DLSS5.ini");
        config = NeuralIniSet(config, "GENERAL", "StartupPresetPath", "");
        config = NeuralIniSet(config, "GENERAL", "NoReloadOnInit", "0");
        for (const auto& pair : {std::pair{"EnableHooks", "2"}, {"NeuralUplift", "1"}, {"NREnableUpscaling", "0"}}) config = NeuralIniSet(config, "RenoDX.DLSS5", pair.first, pair.second);
        const std::string preset = "Techniques=vort_MotionEffects@vort_Motion.fx,DLSS5_Feed@DLSS5_Feed.fx\r\nTechniqueSorting=vort_MotionEffects@vort_Motion.fx,DLSS5_Feed@DLSS5_Feed.fx\r\nPreprocessorDefinitions=DLSS5_MV_PROVIDER=2\r\n[DLSS5_Feed.fx]\r\nPreprocessorDefinitions=DLSS5_MV_PROVIDER=2\r\n";
        WriteNewText(configDir / L"DLSSAllTools-DLSS5.ini", preset);
        files.push_back({configDir / L"DLSSAllTools-DLSS5.ini", L"DLSSAllTools-DLSS5.ini", Sha256(configDir / L"DLSSAllTools-DLSS5.ini"), true});
        const std::string feed = "enabled=1\r\nmode=2\r\nhdr=-1\r\ndepth_inverted=-1\r\nflags=-1\r\nreset_every=0\r\nwarmup_rebuild=180\r\nrebuild=0\r\nlog_frames=3\r\ncreate_delay=60\r\npreset=0\r\nwork_resolution=100\r\nmv_scale_x=1.000\r\nmv_scale_y=1.000\r\nhost_window=0\r\nasync_home=1\r\n";
        WriteNewText(configDir / L"dlss5-feed.cfg", feed);
        files.push_back({configDir / L"dlss5-feed.cfg", L"dlss5-feed.cfg", Sha256(configDir / L"dlss5-feed.cfg"), true});
    }
    WriteNewText(configDir / L"ReShade.ini", config);
    files.push_back({configDir / L"ReShade.ini", L"ReShade.ini", Sha256(configDir / L"ReShade.ini"), true});
    InstallNeuralFiles(game, files, {{"gpu", GpuCode(options.gpu)}, {"route", options.route == NeuralRoute::Native ? "native" : "feeder"}, {"reshadeReused", reuse}}, progress);
    progress(Tr(L"DLSS 5 kuruldu. Oyunda Home → Add-ons → RenoDX DLSS5 bölümünü açın."));
}
void NeuralManager::Uninstall(const Game& game, const Progress& progress) const { RestoreNeuralFiles(game, progress); }
}
