#include "core.hpp"
#include <bcrypt.h>
#include <winhttp.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>

namespace dlss {
namespace {
[[noreturn]] void Fail(const std::string& message) { throw std::runtime_error(message); }
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle(const Handle&) = delete;
    operator HANDLE() const { return h; }
};
struct Internet {
    HINTERNET h;
    explicit Internet(HINTERNET value) : h(value) { if (!h) Fail(TrUtf8("Ağ bağlantısı başlatılamadı.")); }
    ~Internet() { WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
};
const std::vector<std::wstring> Proxies = {
    L"version.dll", L"winmm.dll", L"dinput8.dll", L"dxgi.dll", L"d3d12.dll",
    L"d3d11.dll", L"winhttp.dll", L"wininet.dll", L"dsound.dll", L"d3d9.dll",
    L"d3d10.dll", L"xinput1_3.dll", L"xinput1_4.dll", L"xinput9_1_0.dll",
    L"xinput1_1.dll", L"xinput1_2.dll", L"xinputuap.dll"
};
bool KnownProxy(const std::wstring& name) {
    return std::find(Proxies.begin(), Proxies.end(), name) != Proxies.end();
}
bool SamePath(const fs::path& a, const fs::path& b) {
    return Lower(fs::absolute(a).lexically_normal().wstring()) == Lower(fs::absolute(b).lexically_normal().wstring());
}
void NoReparse(const fs::path& path) {
    auto p = fs::absolute(path).lexically_normal();
    for (;;) {
        auto attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
            Fail(TrUtf8("Bağlantılı klasör veya dosya üzerinde işlem yapılmıyor. Gerçek konumu seçin."));
        auto parent = p.parent_path();
        if (parent == p || parent.empty()) break;
        p = parent;
    }
}
std::string HashHandle(HANDLE file) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        Fail(TrUtf8("SHA-256 başlatılamadı."));
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0); Fail(TrUtf8("SHA-256 oluşturulamadı."));
    }
    std::array<unsigned char, 65536> buffer{};
    std::array<unsigned char, 32> digest{};
    DWORD read = 0; bool ok = true;
    LARGE_INTEGER start{};
    ok = SetFilePointerEx(file, start, nullptr, FILE_BEGIN) != 0;
    while (ok) {
        if (!ReadFile(file, buffer.data(), DWORD(buffer.size()), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, buffer.data(), read, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), DWORD(digest.size()), 0) >= 0;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) Fail(TrUtf8("Dosya doğrulanamadı; dosya kullanımda veya okunamıyor."));
    std::ostringstream out;
    for (auto b : digest) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    return out.str();
}
void WriteExclusive(const fs::path& path, const std::string& data) {
    NoReparse(path);
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.h == INVALID_HANDLE_VALUE) Fail(TrUtf8("Dosya oluşturulamadı. Yazma iznini veya mevcut kurulum kaydını kontrol edin."));
    DWORD written = 0;
    if (!WriteFile(file, data.data(), DWORD(data.size()), &written, nullptr) || written != data.size() || !FlushFileBuffers(file))
        Fail(TrUtf8("Dosya diske kaydedilemedi; işlem tamamlanmadı."));
}
std::wstring Registry(HKEY root, const wchar_t* key, const wchar_t* value, REGSAM view = 0) {
    DWORD size = 0;
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ | view, nullptr, nullptr, &size) != ERROR_SUCCESS || size > 65536) return {};
    std::wstring result(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ | view, nullptr, result.data(), &size) != ERROR_SUCCESS) return {};
    while (!result.empty() && !result.back()) result.pop_back();
    return result;
}
fs::path KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR text = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &text))) Fail(TrUtf8("Windows veri klasörü bulunamadı."));
    fs::path path(text); CoTaskMemFree(text); return path;
}
struct MappedData {
    Handle file, mapping;
    const unsigned char* bytes = nullptr;
    size_t size = 0;
    explicit MappedData(const fs::path& path) {
        file.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER length{};
        if (file.h == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 8ll * 1024 * 1024 * 1024)
            Fail(TrUtf8("Program dosyası okunamadı."));
        size = size_t(length.QuadPart);
        mapping.h = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping.h) Fail(TrUtf8("Program başlığı okunamadı."));
        // Data-only mapping, never SEC_IMAGE/LoadLibrary. Only inspected pages enter memory.
        bytes = static_cast<const unsigned char*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
        if (!bytes) Fail(TrUtf8("Program verisi okunamadı."));
    }
    ~MappedData() { if (bytes) UnmapViewOfFile(bytes); }
};
struct Pe {
    MappedData mapping;
    std::span<const unsigned char> data;
    IMAGE_NT_HEADERS64 header{};
    std::vector<IMAGE_SECTION_HEADER> sections;
    explicit Pe(const fs::path& path) : mapping(path), data(mapping.bytes, mapping.size) {
        auto dos = At<IMAGE_DOS_HEADER>(0);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) Fail(TrUtf8("Geçerli Windows programı değil."));
        header = At<IMAGE_NT_HEADERS64>(size_t(dos.e_lfanew));
        if (header.Signature != IMAGE_NT_SIGNATURE || header.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            header.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || header.FileHeader.NumberOfSections > 96 ||
            header.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) Fail(TrUtf8("64 bit Windows programı gerekli."));
        size_t section = size_t(dos.e_lfanew) + 24 + header.FileHeader.SizeOfOptionalHeader;
        for (unsigned i = 0; i < header.FileHeader.NumberOfSections; ++i)
            sections.push_back(At<IMAGE_SECTION_HEADER>(section + i * sizeof(IMAGE_SECTION_HEADER)));
    }
    template<class T> T At(size_t offset) const {
        if (offset > data.size() || sizeof(T) > data.size() - offset) Fail(TrUtf8("Program başlığı eksik veya bozuk."));
        T result; memcpy(&result, data.data() + offset, sizeof(T)); return result;
    }
    size_t Offset(DWORD rva) const {
        if (rva < header.OptionalHeader.SizeOfHeaders && rva < data.size()) return rva;
        for (auto& s : sections) if (rva >= s.VirtualAddress && rva - s.VirtualAddress < s.SizeOfRawData) {
            const auto offset = size_t(s.PointerToRawData) + rva - s.VirtualAddress;
            if (offset < data.size()) return offset;
        }
        Fail(TrUtf8("Programın DLL tablosu okunamadı."));
    }
    std::string String(DWORD rva) const {
        size_t offset = Offset(rva), end = offset;
        while (end < data.size() && end - offset < 512 && data[end]) ++end;
        if (end >= data.size() || end - offset >= 512) Fail(TrUtf8("Geçersiz DLL adı."));
        std::string result(reinterpret_cast<const char*>(data.data() + offset), end - offset);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return result;
    }
};
std::vector<fs::path> Walk(const fs::path& root, const std::wstring& extension, size_t limit = 15000) {
    std::vector<fs::path> paths;
    if (!fs::is_directory(root)) return paths;
    NoReparse(root);
    std::error_code error;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
    size_t seen = 0;
    while (it != end && seen++ < limit) {
        const auto& entry = *it;
        auto attr = GetFileAttributesW(entry.path().c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT)) it.disable_recursion_pending();
        else if (entry.is_directory(error)) {
            const auto name = Lower(entry.path().filename().wstring());
            if (it.depth() >= 7 || name == L".git" || name == L".vs" || name == L"node_modules") it.disable_recursion_pending();
        } else if (Lower(entry.path().extension().wstring()) == extension) paths.push_back(entry.path());
        it.increment(error);
        if (error) error.clear();
    }
    return paths;
}
bool Evidence(const fs::path& directory) {
    for (const auto& name : {L"sl.dlss_g.dll", L"nvngx_dlssg.dll"})
        if (fs::is_regular_file(directory / name)) return true;
    const auto binaries = directory / L"Plugins";
    if (fs::is_directory(binaries)) for (const auto& dll : Walk(binaries, L".dll", 1500)) {
        auto name = Lower(dll.filename().wstring());
        if (name == L"sl.dlss_g.dll" || name == L"nvngx_dlssg.dll") return true;
    }
    return false;
}
bool GameCandidate(const fs::path& path) {
    const auto name = Lower(path.filename().wstring());
    for (auto word : {L"unins", L"crash", L"report", L"redist", L"setup", L"launcher", L"helper", L"anticheat", L"cef",
        L"bootstrap", L"beservice", L"battleye", L"eac", L"playgtav", L"playgtaiv"})
        if (name.find(word) != std::wstring::npos) return false;
    return IsX64Image(path, false);
}
json Receipt(const Game& game) {
    NoReparse(game.exe);
    const auto path = game.exe.parent_path() / ReceiptName;
    NoReparse(path);
    auto doc = json::parse(ReadText(path, 32768));
    const auto* release = FindRelease(doc.value("version", ""));
    if (doc.value("app", "") != "dlssalltools" || doc.value("schema", 0) != 1 ||
        !release || doc.value("sha256", "") != release->dllHash || !KnownProxy(Wide(doc.value("proxy", ""))) ||
        !SamePath(fs::path(Wide(doc.at("exe").get<std::string>())), game.exe))
        Fail(TrUtf8("Kurulum kaydı doğrulanamadı. Hiçbir oyun dosyası değiştirilmedi."));
    return doc;
}
void ValidateGame(const Game& game) {
    if (game.exe.empty() || !game.exe.is_absolute() || !fs::is_regular_file(game.exe)) Fail(TrUtf8("Oyunun gerçek EXE dosyasını seçin."));
    NoReparse(game.exe);
    if (!IsX64Image(game.exe, false)) Fail(TrUtf8("Yalnızca 64 bit oyun EXE dosyaları destekleniyor."));
    if (GameRunning(game.exe)) Fail(TrUtf8("Önce oyunu tamamen kapatın, sonra yeniden deneyin."));
}
// Hash and delete through the SAME handle. File replacement/writes are blocked while checking.
void RemoveOwned(const fs::path& target, const std::string& expectedHash) {
    NoReparse(target);
    Handle file(CreateFileW(target.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (file.h == INVALID_HANDLE_VALUE) Fail(TrUtf8("Mod dosyası kullanımda veya silme izni yok. Oyunu kapatın."));
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || info.nNumberOfLinks != 1)
        Fail(TrUtf8("Dosya bağlantısı tespit edildi. Otomatik kaldırma durduruldu."));
    if (HashHandle(file) != expectedHash) Fail(TrUtf8("Mod dosyası sonradan değişmiş. Başka bir dosyayı silmemek için kaldırma durduruldu."));
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition))) Fail(TrUtf8("Mod dosyası kaldırılamadı."));
}
std::vector<unsigned char> Download(const std::wstring& url) {
    URL_COMPONENTS parts{sizeof(parts)};
    parts.dwHostNameLength = DWORD(-1); parts.dwUrlPathLength = DWORD(-1); parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) Fail(TrUtf8("Geçersiz indirme adresi."));
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    Internet session(WinHttpOpen(L"DLSSAllTools/0.6", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
    WinHttpSetTimeouts(session, 15000, 15000, 30000, 30000);
    Internet connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
    Internet request(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE));
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr))
        Fail(TrUtf8("İndirme başarısız. İnternet bağlantısını kontrol edin veya doğrulanmış RTXMFG.dll dosyasını içe aktarın."));
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr) || status != 200)
        Fail(TrUtf8("Yayın sunucusu indirmeyi tamamlayamadı."));
    std::vector<unsigned char> bytes;
    std::array<unsigned char, 65536> buffer{};
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), DWORD(buffer.size()), &read)) Fail(TrUtf8("İndirme yarıda kaldı."));
        if (!read) break;
        if (bytes.size() + read > 32 * 1024 * 1024) Fail(TrUtf8("Yayın paketi beklenen boyutu aşıyor."));
        bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + read);
    }
    return bytes;
}
}

std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size) Fail(TrUtf8("Dosya yolunda geçersiz karakter var."));
    std::string result(size, 0); WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), result.data(), size, nullptr, nullptr); return result;
}
std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!size) Fail(TrUtf8("Metin UTF-8 biçiminde okunamadı."));
    std::wstring result(size, 0); MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), result.data(), size); return result;
}
std::wstring Lower(std::wstring text) { std::transform(text.begin(), text.end(), text.begin(), towlower); return text; }
std::string ReadText(const fs::path& file, size_t limit) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) Fail(TrUtf8("Dosya okunamadı: ") + Utf8(file.filename().wstring()));
    stream.seekg(0, std::ios::end); auto size = stream.tellg();
    if (size < 0 || uint64_t(size) > limit) Fail(TrUtf8("Dosya beklenen boyutu aşıyor."));
    stream.seekg(0);
    std::string text(size_t(size), '\0');
    if (!stream.read(text.data(), size)) Fail(TrUtf8("Dosya tamamen okunamadı."));
    return text;
}
std::string Sha256(const fs::path& path) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.h == INVALID_HANDLE_VALUE) Fail(TrUtf8("Doğrulanacak dosya açılamadı."));
    return HashHandle(file);
}
void AtomicJson(const fs::path& path, const json& value) {
    fs::create_directories(path.parent_path()); NoReparse(path);
    auto temp = path; temp += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    WriteExclusive(temp, value.dump(2));
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(temp); Fail(TrUtf8("Kayıt güncellenemedi. Dosya izinlerini kontrol edin."));
    }
}
void NewJson(const fs::path& path, const json& value) {
    NoReparse(path); fs::create_directories(path.parent_path()); WriteExclusive(path, value.dump(2));
}
void RejectLinkedPath(const fs::path& path) { NoReparse(path); }
void RemoveVerifiedFile(const fs::path& path, const std::string& hash) { NoReparse(path); RemoveOwned(path, hash); }
void DownloadVerified(const std::wstring& url, const fs::path& path, const std::string& hash, const Progress& progress) {
    NoReparse(path);
    if (fs::is_regular_file(path) && Sha256(path) == hash) return;
    fs::create_directories(path.parent_path());
    auto temp = path; temp += L".download-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    try {
        URL_COMPONENTS parts{sizeof(parts)};
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) Fail(TrUtf8("Geçersiz indirme adresi."));
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring requestPath(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength) requestPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Internet session(WinHttpOpen(L"DLSSAllTools/0.6", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
        WinHttpSetTimeouts(session, 15000, 15000, 30000, 30000);
        Internet connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
        Internet request(WinHttpOpenRequest(connection, L"GET", requestPath.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE));
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
        if (!WinHttpSendRequest(request, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) Fail(TrUtf8("Bileşen indirilemedi. Bağlantıyı kontrol edip yeniden deneyin."));
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr) || status != 200) Fail(TrUtf8("Bileşen sunucusu indirmeyi tamamlayamadı."));
        {
            Handle file(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (file.h == INVALID_HANDLE_VALUE) Fail(TrUtf8("İndirme dosyası oluşturulamadı."));
            std::array<unsigned char, 65536> buffer{};
            uint64_t total = 0, reported = 0;
            for (;;) {
                DWORD read = 0, written = 0;
                if (!WinHttpReadData(request, buffer.data(), DWORD(buffer.size()), &read)) Fail(TrUtf8("İndirme yarıda kaldı; yeniden deneyebilirsiniz."));
                if (!read) break;
                total += read;
                if (total > 512ull * 1024 * 1024) Fail(TrUtf8("Bileşen beklenen boyutu aşıyor."));
                if (!WriteFile(file, buffer.data(), read, &written, nullptr) || written != read) Fail(TrUtf8("İndirme diske yazılamadı."));
                if (total - reported >= 8 * 1024 * 1024) { reported = total; progress(Tr(L"Bileşen indiriliyor: ") + std::to_wstring(total / 1048576) + L" MB"); }
            }
            if (!FlushFileBuffers(file)) Fail(TrUtf8("İndirme kaydedilemedi."));
        }
        if (Sha256(temp) != hash) Fail(TrUtf8("Bileşenin SHA-256 doğrulaması başarısız. Dosya kullanılmadı."));
        NoReparse(path);
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) Fail(TrUtf8("Bileşen önbelleğe kaydedilemedi."));
    } catch (...) { std::error_code error; fs::remove(temp, error); throw; }
}
void ExtractPinned(const fs::path& archive, const fs::path& directory, const std::vector<std::wstring>& entries) {
    NoReparse(archive); NoReparse(directory); fs::create_directories(directory);
    wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
    const auto tar = fs::path(system) / L"tar.exe";
    std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" + archive.wstring() + L"\" -C \"" + directory.wstring() + L"\"";
    for (const auto& entry : entries) {
        if (entry.empty() || entry[0] == L'-' || entry.find(L'"') != entry.npos || entry.find(L"..") != entry.npos || fs::path(entry).is_absolute()) Fail(TrUtf8("Geçersiz arşiv dosyası."));
        command += L" \"" + entry + L"\"";
    }
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if (!CreateProcessW(tar.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) Fail(TrUtf8("Windows arşiv aracı başlatılamadı."));
    Handle processHandle(process.hProcess), thread(process.hThread);
    if (WaitForSingleObject(processHandle, 60000) != WAIT_OBJECT_0) { TerminateProcess(processHandle, 1); WaitForSingleObject(processHandle, 5000); Fail(TrUtf8("Arşiv açma süresi doldu.")); }
    DWORD code = 1; GetExitCodeProcess(processHandle, &code);
    if (code != 0) Fail(TrUtf8("Bileşen arşivden çıkarılamadı."));
}
fs::path StateDirectory() { return KnownFolder(FOLDERID_LocalAppData) / L"DLSSAllTools"; }

json ParseVdf(const std::string& text) {
    size_t pos = 0;
    auto token = [&]() -> std::string {
        for (;;) {
            while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
            if (pos + 1 < text.size() && text[pos] == '/' && text[pos + 1] == '/') {
                while (pos < text.size() && text[pos] != '\n') ++pos;
            } else break;
        }
        if (pos == text.size()) return {};
        char c = text[pos++];
        if (c == '{' || c == '}') return std::string(1, c);
        if (c != '"') Fail(TrUtf8("Steam kayıt biçimi desteklenmiyor."));
        std::string result;
        while (pos < text.size()) {
            c = text[pos++];
            if (c == '"') return result;
            if (c == '\\' && pos < text.size()) {
                const char next = text[pos];
                if (next == '\\' || next == '"') { result += next; ++pos; } else result += c;
            } else result += c;
        }
        Fail(TrUtf8("Steam kaydı yarım kalmış."));
    };
    std::function<json(unsigned, bool)> parse = [&](unsigned depth, bool nested) {
        if (depth > 32) Fail(TrUtf8("Steam kaydı çok derin."));
        auto result = json::object();
        for (;;) {
            auto key = token();
            if (key.empty()) { if (nested) Fail(TrUtf8("Steam kaydı eksik.")); return result; }
            if (key == "}") { if (!nested) Fail(TrUtf8("Steam kaydı bozuk.")); return result; }
            if (key == "{") Fail(TrUtf8("Steam kaydında anahtar eksik."));
            auto value = token();
            if (value == "}") Fail(TrUtf8("Steam kaydında değer eksik."));
            if (value == "{") result[key] = parse(depth + 1, true); else result[key] = value;
        }
    };
    return parse(0, false);
}

bool IsX64Image(const fs::path& file, bool requireDll) {
    try { Pe pe(file); return bool(pe.header.FileHeader.Characteristics & IMAGE_FILE_DLL) == requireDll; } catch (...) { return false; }
}
std::vector<std::string> PeImports(const fs::path& file) {
    Pe pe(file); std::vector<std::string> imports;
    if (pe.header.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) return imports;
    auto entry = pe.header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!entry.VirtualAddress || !entry.Size) return imports;
    size_t offset = pe.Offset(entry.VirtualAddress);
    for (size_t i = 0; i < std::min<size_t>(entry.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR), 4096); ++i) {
        auto descriptor = pe.At<IMAGE_IMPORT_DESCRIPTOR>(offset + i * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if (!descriptor.Name) break;
        imports.push_back(pe.String(descriptor.Name));
    }
    return imports;
}
std::vector<fs::path> FindExecutables(const fs::path& root) {
    auto paths = Walk(root, L".exe");
    std::erase_if(paths, [](const fs::path& p) { return !GameCandidate(p); });
    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
        const bool ae = Evidence(a.parent_path()), be = Evidence(b.parent_path());
        if (ae != be) return ae > be;
        const bool as = Lower(a.filename().wstring()).find(L"shipping") != std::wstring::npos;
        const bool bs = Lower(b.filename().wstring()).find(L"shipping") != std::wstring::npos;
        return as != bs ? as > bs : a < b;
    });
    return paths;
}

ScanResult DiscoverGames() {
    ScanResult result;
    std::set<std::wstring> seen, libraries;
    auto add = [&](std::wstring name, std::wstring store, fs::path root, fs::path suggested = {}) {
        try {
            if (!fs::is_directory(root)) return;
            root = fs::absolute(root).lexically_normal();
            if (!seen.insert(Lower(root.wstring())).second) return;
            fs::path exe;
            // Known path from upstream README, not a compatibility claim.
            if (fs::is_regular_file(root / L"bin/x64_dx12/witcher3.exe")) exe = root / L"bin/x64_dx12/witcher3.exe";
            else if (fs::is_regular_file(root / L"bin/x64/Cyberpunk2077.exe")) exe = root / L"bin/x64/Cyberpunk2077.exe";
            else {
                auto candidates = FindExecutables(root);
                if (candidates.size() == 1) exe = candidates.front();
                else if (!candidates.empty() && Evidence(candidates.front().parent_path()) &&
                    (candidates.size() == 1 || !Evidence(candidates[1].parent_path()))) exe = candidates.front();
                else if (!suggested.empty() && fs::is_regular_file(suggested) && GameCandidate(suggested)) exe = suggested;
            }
            result.games.push_back({std::move(name), std::move(store), root, exe});
        } catch (const std::exception& e) { result.warnings.push_back(name + L": " + Wide(e.what())); }
    };
    std::set<std::wstring> steamRoots;
    for (auto path : {Registry(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
        Registry(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"InstallPath", RRF_SUBKEY_WOW6432KEY)})
        if (!path.empty()) steamRoots.insert(path);
    auto defaultSteam = KnownFolder(FOLDERID_ProgramFilesX86) / L"Steam";
    if (fs::is_directory(defaultSteam)) steamRoots.insert(defaultSteam.wstring());
    for (const auto& steam : steamRoots) {
        libraries.insert(steam);
        try {
            auto file = fs::path(steam) / L"steamapps/libraryfolders.vdf";
            if (!fs::exists(file)) continue;
            auto doc = ParseVdf(ReadText(file));
            if (doc.contains("libraryfolders")) for (const auto& item : doc["libraryfolders"].items()) {
                if (item.value().is_object() && item.value().contains("path")) libraries.insert(Wide(item.value()["path"].get<std::string>()));
                else if (item.value().is_string() && !item.key().empty() && std::isdigit(static_cast<unsigned char>(item.key()[0])))
                    libraries.insert(Wide(item.value().get<std::string>()));
            }
        } catch (const std::exception& e) { result.warnings.push_back(L"Steam: " + Wide(e.what())); }
    }
    for (const auto& library : libraries) {
        auto apps = fs::path(library) / L"steamapps";
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(apps, fs::directory_options::skip_permission_denied, error)) {
            if (entry.path().extension() != L".acf") continue;
            try {
                auto doc = ParseVdf(ReadText(entry.path())).at("AppState");
                auto relative = fs::path(Wide(doc.at("installdir").get<std::string>()));
                if (relative.has_parent_path() || relative.is_absolute()) continue;
                add(Wide(doc.at("name").get<std::string>()), L"Steam", apps / L"common" / relative);
            } catch (const std::exception& e) { result.warnings.push_back(Tr(L"Steam kaydı: ") + Wide(e.what())); }
        }
    }
    const auto epic = KnownFolder(FOLDERID_ProgramData) / L"Epic/EpicGamesLauncher/Data/Manifests";
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(epic, fs::directory_options::skip_permission_denied, error)) {
        if (entry.path().extension() != L".item") continue;
        try {
            auto doc = json::parse(ReadText(entry.path()));
            if (doc.value("bIsIncompleteInstall", false)) continue;
            fs::path root(Wide(doc.at("InstallLocation").get<std::string>()));
            fs::path launch(Wide(doc.value("LaunchExecutable", "")));
            auto suggested = (root / launch).lexically_normal();
            // Do not accept an Epic manifest pointing outside the declared game directory.
            auto relative = suggested.lexically_relative(root.lexically_normal());
            if (relative.empty() || *relative.begin() == L"..") suggested.clear();
            add(Wide(doc.at("DisplayName").get<std::string>()), L"Epic Games", root, suggested);
        } catch (const std::exception& e) { result.warnings.push_back(Tr(L"Epic kaydı: ") + Wide(e.what())); }
    }
    std::sort(result.games.begin(), result.games.end(), [](const Game& a, const Game& b) { return Lower(a.name) < Lower(b.name); });
    return result;
}

bool GameRunning(const fs::path& exe) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.h == INVALID_HANDLE_VALUE) Fail(TrUtf8("Çalışan oyunlar kontrol edilemedi."));
    PROCESSENTRY32W entry{sizeof(entry)};
    if (Process32FirstW(snapshot, &entry)) do {
        if (Lower(entry.szExeFile) != Lower(exe.filename().wstring())) continue;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (!process.h) return true; // Same name but inaccessible: fail closed.
        std::array<wchar_t, 32768> path{}; DWORD length = DWORD(path.size());
        if (!QueryFullProcessImageNameW(process, 0, path.data(), &length) || SamePath(path.data(), exe)) return true;
    } while (Process32NextW(snapshot, &entry));
    return false;
}

Inspection Inspect(const Game& game) {
    Inspection result;
    if (game.exe.empty()) { result.message = Tr(L"Gerçek oyun EXE dosyasını seçin."); return result; }
    try {
        NoReparse(game.exe);
        const auto directory = game.exe.parent_path();
        if (fs::exists(directory / ReceiptName)) {
            auto doc = Receipt(game); result.proxy = Wide(doc.at("proxy").get<std::string>());
            auto target = directory / result.proxy;
            if (fs::exists(target)) {
                NoReparse(target);
                if (Sha256(target) == doc.at("sha256").get<std::string>()) { result.installed = true; result.message = Tr(L"Kurulu · ") + Wide(doc.value("version", "")); }
                else { result.conflict = true; result.message = Tr(L"Kurulu dosya değişmiş. Otomatik işlem durduruldu."); }
            } else { result.recoverable = true; result.message = Tr(L"Eksik / yarım kurulum kaydı. Kaldır ile temizleyebilirsiniz."); }
            return result;
        }
        if(fs::exists(directory/L".dlssalltools-mfg-sm86")){result.conflict=true;result.message=Tr(L"RTX 20/30 MFG kurulu; önce kendi Kaldır düğmesiyle geri alın.");return result;}
        if (!fs::is_regular_file(game.exe)) { result.conflict = true; result.message = Tr(L"Oyun dosyası taşınmış veya kaldırılmış."); return result; }
        if (!IsX64Image(game.exe, false)) { result.message = Tr(L"64 bit oyun EXE dosyası gerekli."); return result; }
        for (auto legacy : {L"RTX40MFGCore.dll", L"RTX40MFG.asi", L"RTX40MFG-UI.addon64", L"RTX30MFGCore.dll", L"dlssg_sm86.ini", L"RTXMFG.dll", L"plugins/RTX40MFG.asi"})
            if (fs::exists(directory / legacy)) { result.conflict = true; result.message = Tr(L"Önce mevcut RTXMFG kurulumunu kaldırın."); return result; }
        // Never adopt an unknown/manual install as one owned by this manager.
        for (const auto& proxy : Proxies) if (fs::is_regular_file(directory / proxy)) {
            const auto hash = Sha256(directory / proxy);
            for (const auto& release : Releases) if (hash == release.dllHash) {
                result.conflict = true; result.message = Tr(L"Elle kurulmuş RTXMFG bulundu. Dosyaya dokunulmadı."); return result;
            }
        }
        auto imports = PeImports(game.exe);
        result.fgEvidence = Evidence(directory);
        if (!result.fgEvidence) {
            // Common Unreal Engine location, bounded and relative to the installation.
            if (!game.root.empty() && fs::is_directory(game.root / L"Engine/Plugins"))
                for (const auto& dll : Walk(game.root / L"Engine/Plugins", L".dll", 4000)) {
                    auto name = Lower(dll.filename().wstring());
                    if (name == L"sl.dlss_g.dll" || name == L"nvngx_dlssg.dll") { result.fgEvidence = true; break; }
                }
        }
        // Upstream explicitly documents this route for The Witcher 3 DX12.
        if (Lower(game.exe.filename().wstring()) == L"witcher3.exe" && Lower(directory.filename().wstring()) == L"x64_dx12" && !fs::exists(directory / L"winmm.dll"))
            result.proxy = L"winmm.dll";
        else for (const auto& proxy : Proxies) if (!fs::exists(directory / proxy) &&
            std::find(imports.begin(), imports.end(), Utf8(proxy)) != imports.end()) { result.proxy = proxy; break; }
        if (!result.fgEvidence) result.message = Tr(L"DLSS Frame Generation bileşeni bulunamadı. Otomatik kurulum kapalı.");
        else if (result.proxy.empty()) result.message = Tr(L"Boş ve uygun bir yükleme yolu bulunamadı. Oyun profili gerekiyor.");
        else result.message = Tr(L"Kurulabilir · oyun içi uyumluluk henüz doğrulanmadı");
    } catch (const std::exception& e) { result.conflict = true; result.message = Wide(e.what()); }
    return result;
}

std::wstring GpuDescription() {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return Tr(L"Ekran kartı okunamadı");
    std::wstring fallback;
    for (UINT i = 0;; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        if (!adapter) continue;
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (desc.VendorId == 0x10DE) return desc.Description;
        if (fallback.empty()) fallback = desc.Description;
    }
    return fallback.empty() ? Tr(L"Ekran kartı bulunamadı") : fallback;
}

const Release* FindRelease(const std::string& tag) {
    for (const auto& release : Releases) if (tag == release.tag) return &release;
    return nullptr;
}
Manager::Manager(fs::path state, std::string release) : state_(std::move(state)) {
    NoReparse(state_); fs::create_directories(state_);
    if (release.empty()) {
        const auto preferences = state_ / L"preferences.json";
        if (fs::exists(preferences)) {
            try { NoReparse(preferences); release = json::parse(ReadText(preferences, 4096)).value("release", ""); }
            catch (const std::exception&) { /* A damaged preference must not prevent opening the manager. */ }
        }
    }
    if (const auto* selected = FindRelease(release)) release_ = selected;
    else if (!release.empty()) Fail(TrUtf8("Bilinmeyen mod sürümü. Desteklenen sürümlerden birini seçin."));
}
void Manager::SelectRelease(const std::string& tag) {
    const auto* selected = FindRelease(tag);
    if (!selected) Fail(TrUtf8("Bilinmeyen mod sürümü."));
    AtomicJson(state_ / L"preferences.json", {{"release", tag}});
    release_ = selected;
}
fs::path Manager::Payload() const { return state_ / L"packages" / Wide(release_->tag) / L"RTXMFG.dll"; }
bool Manager::PackageReady() const { try { NoReparse(Payload()); return fs::is_regular_file(Payload()) && Sha256(Payload()) == release_->dllHash; } catch (...) { return false; } }
void Manager::ImportPackage(const fs::path& dll) const {
    NoReparse(dll);
    if (Sha256(dll) != release_->dllHash || !IsX64Image(dll, true)) Fail(TrUtf8("DLL seçili sürümle eşleşmiyor. ") + std::string(release_->tag) + TrUtf8(" paketindeki RTXMFG.dll gerekli."));
    if (SamePath(dll, Payload())) return;
    fs::create_directories(Payload().parent_path()); NoReparse(Payload());
    auto temp = Payload(); temp += L".incoming-" + std::to_wstring(GetCurrentProcessId());
    if (!CopyFileW(dll.c_str(), temp.c_str(), TRUE)) Fail(TrUtf8("Paket önbelleğe kopyalanamadı."));
    try {
        if (Sha256(temp) != release_->dllHash) Fail(TrUtf8("Kopyalanan paket doğrulanamadı."));
        if (!MoveFileExW(temp.c_str(), Payload().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) Fail(TrUtf8("Paket kaydedilemedi."));
    } catch (...) { std::error_code e; fs::remove(temp, e); throw; }
}
void Manager::PreparePackage(const Progress& progress) const {
    if (PackageReady()) { progress(Tr(L"Doğrulanmış mod paketi hazır.")); return; }
    progress(Tr(L"Resmi RTXMFG paketi indiriliyor…"));
    const auto tag = Wide(release_->tag);
    const auto bytes = Download(L"https://github.com/dashdogy/RTX40MFG-Unlock/releases/download/" + tag + L"/RTXMFG-" + tag + L".zip");
    auto folder = state_ / (L"download-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    NoReparse(folder);
    if (!fs::create_directory(folder)) Fail(TrUtf8("İndirme klasörü oluşturulamadı."));
    try {
        const auto zip = folder / L"package.zip";
        WriteExclusive(zip, std::string(bytes.begin(), bytes.end()));
        if (Sha256(zip) != release_->zipHash) Fail(TrUtf8("İndirilen paketin SHA-256 doğrulaması başarısız. Paket kullanılmadı."));
        progress(Tr(L"Paket doğrulandı; mod dosyası hazırlanıyor…"));
        wchar_t system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
        fs::path tar = fs::path(system) / L"tar.exe";
        if (!fs::is_regular_file(tar)) Fail(TrUtf8("Windows arşiv aracı bulunamadı. DLL içe aktarma seçeneğini kullanın."));
        // Only the fixed root entry is extracted from a pinned, hash-verified ZIP.
        std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" + zip.wstring() + L"\" -C \"" + folder.wstring() + L"\" RTXMFG.dll";
        STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
        if (!CreateProcessW(tar.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
            Fail(TrUtf8("Paket açılamadı."));
        Handle processHandle(process.hProcess), thread(process.hThread);
        auto wait = WaitForSingleObject(processHandle, 30000);
        if (wait != WAIT_OBJECT_0) { TerminateProcess(processHandle, 1); WaitForSingleObject(processHandle, 5000); Fail(TrUtf8("Paket açma işlemi zaman aşımına uğradı.")); }
        DWORD code = 1; GetExitCodeProcess(processHandle, &code);
        if (code != 0) Fail(TrUtf8("Arşivden RTXMFG.dll çıkarılamadı."));
        ImportPackage(folder / L"RTXMFG.dll");
        progress(L"RTXMFG " + tag + Tr(L" hazır. SHA-256 doğrulandı."));
        fs::remove_all(folder);
    } catch (...) { std::error_code error; fs::remove_all(folder, error); throw; }
}
std::vector<Game> Manager::LoadGames() const {
    std::vector<Game> games;
    auto path = state_ / L"library.json";
    if (!fs::exists(path)) return games;
    const auto doc = json::parse(ReadText(path));
    for (const auto& item : doc.at("games")) games.push_back({Wide(item.at("name").get<std::string>()),
        Wide(item.at("store").get<std::string>()), fs::path(Wide(item.at("root").get<std::string>())), fs::path(Wide(item.at("exe").get<std::string>()))});
    return games;
}
void Manager::SaveGames(const std::vector<Game>& games) const {
    json items = json::array();
    for (const auto& game : games) items.push_back({{"name", Utf8(game.name)}, {"store", Utf8(game.store)},
        {"root", Utf8(game.root.wstring())}, {"exe", Utf8(game.exe.wstring())}});
    AtomicJson(state_ / L"library.json", {{"schema", 1}, {"games", items}});
}
void Manager::Install(const Game& game) const {
    ValidateGame(game);
    if (!PackageReady()) Fail(TrUtf8("Önce mod paketini hazırlayın."));
    auto inspection = Inspect(game);
    if (inspection.installed) Fail(TrUtf8("Bu oyunda mod zaten kurulu. Sürüm değiştirmek için önce Kaldır, ardından Kur seçin."));
    if (inspection.conflict || inspection.recoverable || !inspection.fgEvidence || inspection.proxy.empty()) Fail(Utf8(inspection.message));
    auto directory = game.exe.parent_path();
    const auto target = directory / inspection.proxy, receipt = directory / ReceiptName;
    NoReparse(target);
    const auto doc = json{{"app", "dlssalltools"}, {"schema", 1}, {"exe", Utf8(fs::absolute(game.exe).lexically_normal().wstring())},
        {"proxy", Utf8(inspection.proxy)}, {"sha256", release_->dllHash}, {"version", release_->tag}, {"state", "pending"}};
    // Journal is durable BEFORE copying. Interrupted installs remain visible/recoverable.
    WriteExclusive(receipt, doc.dump(2));
    if (!CopyFileW(Payload().c_str(), target.c_str(), TRUE)) {
        // Keep journal when a target appeared: ownership is deliberately not assumed.
        if (!fs::exists(target)) fs::remove(receipt);
        Fail(TrUtf8("Kurulum tamamlanamadı. Dosya çakışması veya yazma izni sorunu var."));
    }
    if (Sha256(target) != release_->dllHash) Fail(TrUtf8("Kopya doğrulanamadı. Kurulum kaydı korundu; dosyayı kontrol edin."));
    auto complete = doc; complete["state"] = "installed";
    AtomicJson(receipt, complete);
}
void Manager::Uninstall(const Game& game) const {
    // Removal still works after the game EXE has been deleted by a game update.
    if (game.exe.empty() || !game.exe.is_absolute()) Fail(TrUtf8("Kayıtlı oyun konumu yok."));
    NoReparse(game.exe);
    if (GameRunning(game.exe)) Fail(TrUtf8("Önce oyunu tamamen kapatın."));
    auto doc = Receipt(game);
    const auto directory = game.exe.parent_path();
    const auto target = directory / Wide(doc.at("proxy").get<std::string>());
    if (fs::exists(target)) RemoveOwned(target, doc.at("sha256").get<std::string>());
    // Settings belong to the user. They are intentionally preserved.
    if (!fs::remove(directory / ReceiptName)) Fail(TrUtf8("Mod kaldırıldı ancak kurulum kaydı temizlenemedi."));
}
}
