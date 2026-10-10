#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "localization.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "../third_party/json.hpp"

namespace dlss {
namespace fs = std::filesystem;
using json = nlohmann::json;
inline constexpr char Version[] = "0.6.0";
struct Release {
    const char* tag;
    const wchar_t* label;
    const char* dllHash;
    const char* zipHash;
};
inline constexpr Release Releases[] = {
    {"v1.4.1-hotfix.1", L"1.4.1 Hotfix 1", "cba32d595b0c6026e115a1e4b1db8fa34dc775d73700c8e79104138809cbeee8",
        "3ef3a7e8e331170c355765b7c197eeb94823854c28028e71ecf47bee345e2d16"},
    {"v1.3.3-hotfix.2", L"1.3.3 Hotfix 2 (önceki)", "e9ca3587854eeb723e0579f7ddf6cfb1e6cf4bed79b0d75bc716003ed98fe040",
        "edd671188b24520b2c1a313ce55d2184538e7a561263408d3460b5663d60dca5"}
};
const Release* FindRelease(const std::string& tag);
inline constexpr wchar_t ReceiptName[] = L".dlssalltools-install.json";

struct Game {
    std::wstring name, store;
    fs::path root, exe;
};
struct Inspection {
    bool installed = false, recoverable = false, conflict = false, fgEvidence = false;
    std::wstring proxy, message;
};
struct ScanResult { std::vector<Game> games; std::vector<std::wstring> warnings; };
using Progress = std::function<void(const std::wstring&)>;

std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);
std::wstring Lower(std::wstring text);
std::string ReadText(const fs::path& file, size_t limit = 4 * 1024 * 1024);
void AtomicJson(const fs::path& path, const json& value);
void NewJson(const fs::path& path, const json& value);
void RejectLinkedPath(const fs::path& path);
void DownloadVerified(const std::wstring& url, const fs::path& path, const std::string& hash, const Progress& progress);
void ExtractPinned(const fs::path& archive, const fs::path& directory, const std::vector<std::wstring>& entries);
void RemoveVerifiedFile(const fs::path& path, const std::string& hash);
std::string Sha256(const fs::path& path);
fs::path StateDirectory();
json ParseVdf(const std::string& text);
std::vector<std::string> PeImports(const fs::path& file);
bool IsX64Image(const fs::path& file, bool requireDll);
std::vector<fs::path> FindExecutables(const fs::path& root);
ScanResult DiscoverGames();
Inspection Inspect(const Game& game);
bool GameRunning(const fs::path& exe);
std::wstring GpuDescription();

class Manager {
public:
    explicit Manager(fs::path state = StateDirectory(), std::string release = {});
    const Release& SelectedRelease() const { return *release_; }
    void SelectRelease(const std::string& tag);
    fs::path Payload() const;
    bool PackageReady() const;
    void PreparePackage(const Progress& progress) const;
    void ImportPackage(const fs::path& dll) const;
    std::vector<Game> LoadGames() const;
    void SaveGames(const std::vector<Game>& games) const;
    void Install(const Game& game) const;
    void Uninstall(const Game& game) const;
    const fs::path& State() const { return state_; }
private:
    fs::path state_;
    const Release* release_ = &Releases[1];
};
}
