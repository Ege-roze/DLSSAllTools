#include "sm86.hpp"
#include "core.hpp"
#include "preset.hpp"
#include "neural.hpp"
#include <fstream>
#include <functional>
#include <sstream>

using namespace dlss;
namespace {
void Require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void MustFail(const std::function<void()>& action) {
    bool failed = false; try { action(); } catch (const std::exception&) { failed = true; }
    Require(failed, "An unsafe operation unexpectedly succeeded");
}
void Bytes(const fs::path& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary); file.write(data.data(), std::streamsize(data.size()));
    if (!file) throw std::runtime_error("Fixture write failed");
}
void Fixture(const fs::path& file, const std::string& imported = "version.dll") {
    std::vector<unsigned char> data(4096);
    auto put = [&](size_t offset, const auto& value) { memcpy(data.data() + offset, &value, sizeof(value)); };
    IMAGE_DOS_HEADER dos{}; dos.e_magic = IMAGE_DOS_SIGNATURE; dos.e_lfanew = 0x80; put(0, dos);
    IMAGE_NT_HEADERS64 nt{}; nt.Signature = IMAGE_NT_SIGNATURE; nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt.FileHeader.NumberOfSections = 1; nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
    nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC; nt.OptionalHeader.SizeOfHeaders = 0x400;
    nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = {0x1000, 40}; put(0x80, nt);
    IMAGE_SECTION_HEADER section{}; section.VirtualAddress = 0x1000; section.PointerToRawData = 0x400; section.SizeOfRawData = 0xC00;
    put(0x80 + sizeof(nt), section);
    IMAGE_IMPORT_DESCRIPTOR descriptor{}; descriptor.Name = 0x1100; put(0x400, descriptor);
    memcpy(data.data() + 0x500, imported.c_str(), imported.size() + 1);
    Bytes(file, std::string(data.begin(), data.end()));
}
class FakeProfile final : public ProfileStore {
public:
    DriverSnapshot live{L"Test profile", {L"TestGame.exe"}, {{{true, false, 0}, {true, false, 0}}}, false};
    DriverSnapshot staged = live;
    std::array<DriverValue, 2> inherited{{{true, false, 0}, {true, false, 0}}};
    bool failSave = false, partialSave = false, ignoreSave = false, failStage = false;
    unsigned saves = 0;
    uint32_t unrelated = 77;
    std::function<void()> beforeWrite;
    DriverSnapshot Read() override { return staged; }
    void Reload() override { staged = live; }
    void Stage(size_t index, std::optional<uint32_t> value) override {
        if (beforeWrite) beforeWrite();
        if (failStage && index == 1) throw std::runtime_error("Simulated second-setting failure");
        staged.settings.at(index) = value ? DriverValue{true, true, *value} : inherited.at(index);
    }
    void Save() override {
        ++saves;
        if (partialSave) { live.settings[0] = staged.settings[0]; throw std::runtime_error("Simulated interrupted save"); }
        if (failSave) throw std::runtime_error("Simulated permission failure");
        if (!ignoreSave) live = staged;
    }
};
}

int RunLanguageTests(const fs::path& directory) {
    fs::create_directories(directory);
    const auto root=fs::absolute(directory)/(L"fixtures-"+std::to_wstring(GetCurrentProcessId()));
    fs::create_directories(root);
    std::ofstream report(directory/L"language-tests.txt",std::ios::binary);
    size_t passed=0,failed=0;
    auto test=[&](const char* name,const std::function<void()>& run){
        SetLanguage(Language::Turkish);
        try{run();report<<"PASS  "<<name<<'\n';++passed;}
        catch(const std::exception& e){report<<"FAIL  "<<name<<": "<<e.what()<<'\n';++failed;}
        report.flush();
    };
    test("Compiled catalog has unique complete translations",[]{Require(ValidateTranslations()&&TranslationCount()>=307,"Incomplete catalog");});
    test("Three languages preserve Unicode in wide and UTF-8 messages",[]{
        const wchar_t* expected[]{L"Kapat",L"Close",L"关闭"};
        for(size_t i=0;i<std::size(Languages);++i){SetLanguage(Languages[i]);Require(std::wstring(Tr(L"Kapat"))==expected[i],"Wrong caption");Require(Wide(TrUtf8("Kapat"))==expected[i],"UTF-8 damaged");}
    });
    test("Preference survives reopening for each language",[&]{for(auto language:Languages){SaveLanguage(root,language);SetLanguage(Language::Turkish);LoadLanguage(root);Require(CurrentLanguage()==language,"Preference lost");}});
    test("Language and release preferences remain independent",[&]{
        Manager manager(root);manager.SelectRelease(Releases[0].tag);const auto before=ReadText(root/L"preferences.json");
        SaveLanguage(root,Language::English);Require(ReadText(root/L"preferences.json")==before,"Release preference changed");
        const auto ui=ReadText(root/L"ui-preferences.json");manager.SelectRelease(Releases[1].tag);Require(ReadText(root/L"ui-preferences.json")==ui,"Language preference changed");
        LoadLanguage(root);Require(CurrentLanguage()==Language::English,"Language not restored");
    });
    test("Missing preference defaults to Turkish without creating a file",[&]{SetLanguage(Language::English);LoadLanguage(root/L"missing");Require(CurrentLanguage()==Language::Turkish&&!fs::exists(root/L"missing"),"Missing preference not handled");});
    test("Malformed oversized or unknown preferences fall back safely",[&]{
        for(const auto& text:{std::string("{broken"),std::string("{\"language\":\"xx\"}"),std::string("{\"language\":7}"),std::string(5000,'x')}){
            Bytes(root/L"ui-preferences.json",text);SetLanguage(Language::English);LoadLanguage(root);Require(CurrentLanguage()==Language::Turkish,"Bad preference accepted");Require(ReadText(root/L"ui-preferences.json")==text,"Load modified preference");
        }
    });
    test("Unicode preference folder works",[&]{const auto path=root/L"语言 ayarları";SaveLanguage(path,Language::SimplifiedChinese);SetLanguage(Language::English);LoadLanguage(path);Require(CurrentLanguage()==Language::SimplifiedChinese,"Unicode path failed");});
    test("Failed save keeps active language and blocked path intact",[&]{const auto path=root/L"blocked";Bytes(path,"existing file");SetLanguage(Language::English);MustFail([&]{SaveLanguage(path,Language::SimplifiedChinese);});Require(CurrentLanguage()==Language::English&&ReadText(path)=="existing file","Failed save changed state");});
    test("Invalid language cannot change memory or disk",[&]{SaveLanguage(root,Language::English);const auto before=ReadText(root/L"ui-preferences.json");MustFail([&]{SaveLanguage(root,static_cast<Language>(99));});Require(CurrentLanguage()==Language::English&&ReadText(root/L"ui-preferences.json")==before,"Invalid language changed state");});
    test("Game identities and paths survive all language changes",[&]{
        Manager manager(root);const Game game{L"游戏 Türkçe Game",L"Elle eklendi",root/L"Kapat",root/L"Kapat"/L"游戏.exe"};manager.SaveGames({game});const auto before=ReadText(root/L"library.json");
        for(auto language:Languages){SaveLanguage(root,language);const auto games=manager.LoadGames();Require(games.size()==1&&games[0].name==game.name&&games[0].store==game.store&&games[0].exe==game.exe,"Game data translated");Require(ReadText(root/L"library.json")==before,"Game file changed");const auto path=game.exe.wstring();Require(std::wstring(Tr(path.c_str()))==path,"Unknown path translated");}
    });
    test("GPU detection and labels work in each language",[]{for(auto language:Languages){SetLanguage(language);Require(ParseGpuSeries(L"NVIDIA GeForce RTX 3060 Laptop GPU")==GpuSeries::RTX30,"GPU parsing changed");Require(GpuLabel(GpuSeries::RTX20)==std::wstring(L"RTX 20")+Tr(L" serisi"),"GPU label failed");try{GpuCode(GpuSeries::Unknown);Require(false,"Unknown GPU accepted");}catch(const std::runtime_error& e){Require(std::string(e.what())==TrUtf8("Önce RTX 20, 30, 40 veya 50 serisini seçin."),"GPU error not translated");}}});
    test("Chinese font is selected without changing other locales",[]{SetLanguage(Language::SimplifiedChinese);Require(std::wstring(UiFontName())==L"Microsoft YaHei UI","CJK font missing");SetLanguage(Language::English);Require(std::wstring(UiFontName())==L"Segoe UI","English font changed");});
    SetLanguage(Language::Turkish);
    report<<"\n"<<passed<<" passed, "<<failed<<" failed.\n"<<TranslationCount()<<" compiled translation entries. No game or driver changes.\n";report.close();
    if(!failed)fs::remove_all(root);return failed?1:0;
}

int RunSelfTests(const fs::path& directory, const fs::path& payload, const fs::path& oldPayload) {
    fs::create_directories(directory);
    const auto root = fs::absolute(directory) / (L"fixtures-" + std::to_wstring(GetCurrentProcessId()));
    std::ofstream report(directory / L"test-results.txt", std::ios::binary);
    size_t passed = 0, failed = 0;
    auto test = [&](const char* name, const std::function<void()>& run) {
        try { run(); report << "PASS  " << name << '\n'; ++passed; }
        catch (const std::exception& e) { report << "FAIL  " << name << ": " << e.what() << '\n'; ++failed; }
        report.flush();
    };
    try {
        Require(!fs::exists(root), "Test directory must be fresh"); fs::create_directories(root);
        Manager manager(root / L"state", Releases[0].tag);
        const auto gameDir = root / L"Oyun Türkçe boşluklu"; fs::create_directories(gameDir);
        Game game{L"Test oyunu", L"Test", gameDir, gameDir / L"TestGame.exe"};
        Fixture(game.exe); Bytes(gameDir / L"sl.dlss_g.dll", "fixture only, never loaded");
        test("UTF-8 Turkish path round trip", [&] { Require(Wide(Utf8(game.exe.wstring())) == game.exe.wstring(), "UTF-8 round trip failed"); });
        test("Steam VDF multi-library escaped paths", [&] {
            auto doc = ParseVdf(R"("libraryfolders" { "0" { "path" "D:\\Steam Library" "apps" { "1091500" "1" } } "1" { "path" "E:\\Oyunlar" } })");
            Require(doc["libraryfolders"]["0"]["path"] == "D:\\Steam Library", "VDF path mismatch");
            Require(doc["libraryfolders"]["1"]["path"] == "E:\\Oyunlar", "Second library missing");
        });
        test("Truncated VDF is rejected", [&] { MustFail([] { ParseVdf("\"AppState\" { \"name\" \"test\""); }); });
        test("PE import table is read without loading executable", [&] { Require(IsX64Image(game.exe, false) && PeImports(game.exe).at(0) == "version.dll", "PE route failed"); });
        test("Malformed PE cannot be used as game", [&] { auto path = root / L"bad.exe"; Bytes(path, "MZbad"); Require(!IsX64Image(path, false), "Bad PE accepted"); });
        test("Game discovery excludes launchers and helper services", [&] {
            const auto folder = root / L"discovery"; fs::create_directory(folder);
            for (auto name : {L"GTA5_Enhanced.exe", L"PlayGTAV.exe", L"FortniteBootstrapper.exe", L"BEService_x64.exe", L"CrashReport.exe"}) Fixture(folder / name);
            auto found = FindExecutables(folder); Require(found.size() == 1 && found.front().filename() == L"GTA5_Enhanced.exe", "Launcher/helper was selected");
        });
        test("Official DLL import and SHA-256 verification", [&] { manager.ImportPackage(payload); Require(manager.PackageReady(), "Official package rejected"); });
        test("Unknown DLL is rejected without changing package", [&] {
            auto bad = root / L"unknown.dll"; Bytes(bad, "unknown"); MustFail([&] { manager.ImportPackage(bad); }); Require(manager.PackageReady(), "Good package overwritten");
        });
        test("Library persists Unicode paths and names", [&] {
            manager.SaveGames({game}); const auto loaded = manager.LoadGames();
            Require(loaded.size() == 1 && loaded[0].exe == game.exe && loaded[0].name == game.name, "Library changed");
        });
        test("Free imported proxy route is selected", [&] {
            auto state = Inspect(game); Require(!state.conflict && state.fgEvidence && state.proxy == L"version.dll", "Route unavailable");
        });
        const auto originalExeHash = Sha256(game.exe);
        test("Install verified payload and durable receipt", [&] {
            manager.Install(game); Require(Inspect(game).installed, "Installed status missing");
            Require(Sha256(gameDir / L"version.dll") == Releases[0].dllHash && Sha256(game.exe) == originalExeHash, "Game changed");
            const auto doc = json::parse(ReadText(gameDir / ReceiptName)); Require(doc.at("state") == "installed", "Receipt not committed");
        });
        test("Duplicate install is rejected", [&] { MustFail([&] { manager.Install(game); }); Require(Inspect(game).installed, "Installed file changed"); });
        test("Uninstall keeps game, settings and unrelated files", [&] {
            Bytes(gameDir / L"RTXMFG-Universal.json", "user settings"); Bytes(gameDir / L"other-mod.dll", "unrelated");
            manager.Uninstall(game);
            Require(!fs::exists(gameDir / L"version.dll") && !fs::exists(gameDir / ReceiptName), "Mod not removed");
            Require(ReadText(gameDir / L"RTXMFG-Universal.json") == "user settings" && ReadText(gameDir / L"other-mod.dll") == "unrelated", "Other files changed");
            Require(Sha256(game.exe) == originalExeHash, "Game executable changed");
        });
        test("Existing foreign proxy DLL is never overwritten", [&] {
            Bytes(gameDir / L"version.dll", "original-game-dll"); MustFail([&] { manager.Install(game); });
            Require(ReadText(gameDir / L"version.dll") == "original-game-dll", "Original DLL overwritten"); fs::remove(gameDir / L"version.dll");
        });
        test("Changed installed DLL is never removed", [&] {
            manager.Install(game); Bytes(gameDir / L"version.dll", "modified by another program");
            MustFail([&] { manager.Uninstall(game); }); Require(ReadText(gameDir / L"version.dll") == "modified by another program", "Changed DLL deleted");
            fs::copy_file(payload, gameDir / L"version.dll", fs::copy_options::overwrite_existing); manager.Uninstall(game);
        });
        test("Manual installation is not adopted or deleted", [&] {
            fs::copy_file(payload, gameDir / L"winmm.dll"); Require(Inspect(game).conflict, "Manual install ignored");
            MustFail([&] { manager.Install(game); }); MustFail([&] { manager.Uninstall(game); });
            Require(fs::exists(gameDir / L"winmm.dll"), "Manual installation removed"); fs::remove(gameDir / L"winmm.dll");
        });
        test("Legacy split installation blocks automatic install", [&] {
            fs::create_directories(gameDir / L"plugins"); Bytes(gameDir / L"plugins/RTX40MFG.asi", "legacy");
            MustFail([&] { manager.Install(game); }); Require(fs::exists(gameDir / L"plugins/RTX40MFG.asi"), "Legacy mod touched"); fs::remove(gameDir / L"plugins/RTX40MFG.asi");
        });
        test("Interrupted install with missing DLL can be cleaned", [&] {
            manager.Install(game); fs::remove(gameDir / L"version.dll"); Require(Inspect(game).recoverable, "Recovery status missing"); manager.Uninstall(game);
            Require(!fs::exists(gameDir / ReceiptName), "Orphan receipt left");
        });
        test("Interrupted pending receipt with complete DLL can be removed", [&] {
            manager.Install(game); auto doc = json::parse(ReadText(gameDir / ReceiptName)); doc["state"] = "pending"; AtomicJson(gameDir / ReceiptName, doc);
            Require(Inspect(game).installed, "Pending owned DLL unrecognized"); manager.Uninstall(game);
        });
        test("Forged receipt path cannot delete an unrelated file", [&] {
            manager.Install(game); const auto valid = json::parse(ReadText(gameDir / ReceiptName)); auto bad = valid; bad["proxy"] = "../outside.dll";
            AtomicJson(gameDir / ReceiptName, bad); MustFail([&] { manager.Uninstall(game); }); Require(fs::exists(gameDir / L"version.dll"), "Foreign path accepted");
            AtomicJson(gameDir / ReceiptName, valid); manager.Uninstall(game);
        });
        test("Forged receipt hash cannot authorize another file", [&] {
            manager.Install(game); const auto valid = json::parse(ReadText(gameDir / ReceiptName)); auto bad = valid; bad["sha256"] = "forged";
            AtomicJson(gameDir / ReceiptName, bad); MustFail([&] { manager.Uninstall(game); }); AtomicJson(gameDir / ReceiptName, valid); manager.Uninstall(game);
        });
        test("Locked installed DLL is preserved until released", [&] {
            manager.Install(game);
            HANDLE lock = CreateFileW((gameDir / L"version.dll").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(lock != INVALID_HANDLE_VALUE, "Cannot lock fixture");
            try { MustFail([&] { manager.Uninstall(game); }); } catch (...) { CloseHandle(lock); throw; }
            CloseHandle(lock); Require(fs::exists(gameDir / L"version.dll"), "Locked DLL removed"); manager.Uninstall(game);
        });
        test("Hard-linked owned DLL is not deleted", [&] {
            manager.Install(game); const auto link = root / L"linked.dll"; fs::create_hard_link(gameDir / L"version.dll", link);
            MustFail([&] { manager.Uninstall(game); }); fs::remove(link); manager.Uninstall(game);
        });
        test("Missing Frame Generation component blocks installation", [&] {
            fs::remove(gameDir / L"sl.dlss_g.dll"); MustFail([&] { manager.Install(game); }); Require(!fs::exists(gameDir / L"version.dll"), "Unsupported game modified");
            Bytes(gameDir / L"sl.dlss_g.dll", "fixture");
        });
        test("Missing game executable does not prevent removal", [&] {
            manager.Install(game); fs::remove(game.exe); Require(Inspect(game).installed, "UI cannot remove installation after EXE disappears");
            manager.Uninstall(game); Require(!fs::exists(gameDir / L"version.dll"), "Orphan installation not removed"); Fixture(game.exe);
        });
        test("Corrupt cached DLL blocks install", [&] {
            Bytes(manager.Payload(), "corrupt"); MustFail([&] { manager.Install(game); }); Require(!fs::exists(gameDir / ReceiptName), "Receipt created for corrupt payload"); manager.ImportPackage(payload);
        });
        test("Running executable is detected", [&] {
            wchar_t current[32768]{}; GetModuleFileNameW(nullptr, current, 32768); Require(GameRunning(current), "Current process not detected");
        });
        if (!oldPayload.empty()) {
            test("Fresh manager defaults to previous release", [&] {
                Manager fresh(root / L"fresh-state"); Require(&fresh.SelectedRelease() == &Releases[1], "Unexpected default");
            });
            test("Release choice persists and caches stay separate", [&] {
                const auto currentPath = manager.Payload(); manager.SelectRelease(Releases[1].tag);
                Require(!manager.PackageReady() && currentPath != manager.Payload(), "Cache mixed across releases");
                manager.ImportPackage(oldPayload); Require(manager.PackageReady(), "Previous release rejected");
                Manager reopened(manager.State()); Require(&reopened.SelectedRelease() == &Releases[1] && reopened.PackageReady(), "Selection not restored");
                Require(Sha256(currentPath) == Releases[0].dllHash, "Newer package changed");
            });
            test("Wrong release import is rejected without changing cached DLL", [&] {
                MustFail([&] { manager.ImportPackage(payload); }); Require(manager.PackageReady(), "Previous package overwritten");
            });
            test("Old package installs with matching version and hash receipt", [&] {
                manager.Install(game); const auto doc = json::parse(ReadText(gameDir / ReceiptName));
                Require(doc.at("version") == Releases[1].tag && doc.at("sha256") == Releases[1].dllHash, "Wrong receipt");
                Require(Sha256(gameDir / L"version.dll") == Releases[1].dllHash && Inspect(game).installed, "Previous release not installed");
            });
            test("Selecting another release cannot overwrite installed version", [&] {
                manager.SelectRelease(Releases[0].tag); MustFail([&] { manager.Install(game); });
                Require(Sha256(gameDir / L"version.dll") == Releases[1].dllHash && Inspect(game).installed, "Installed release changed");
            });
            test("Known hash with mismatched version cannot authorize removal", [&] {
                const auto valid = json::parse(ReadText(gameDir / ReceiptName)); auto wrong = valid;
                wrong["version"] = Releases[0].tag; AtomicJson(gameDir / ReceiptName, wrong);
                MustFail([&] { manager.Uninstall(game); }); Require(fs::exists(gameDir / L"version.dll") && Inspect(game).conflict, "Mismatched receipt accepted");
                wrong = valid; wrong["version"] = "v9-untrusted"; AtomicJson(gameDir / ReceiptName, wrong);
                MustFail([&] { manager.Uninstall(game); }); AtomicJson(gameDir / ReceiptName, valid);
            });
            test("Newer selection removes older installation through its own receipt", [&] {
                manager.Uninstall(game); Require(!fs::exists(gameDir / L"version.dll"), "Old installation left behind");
                Require(ReadText(gameDir / L"RTXMFG-Universal.json") == "user settings", "Settings lost");
            });
            test("Older selection removes existing v0.1-compatible newer receipt", [&] {
                manager.Install(game); manager.SelectRelease(Releases[1].tag);
                Require(Inspect(game).installed, "Existing newer receipt unrecognized");
                manager.Uninstall(game); Require(!fs::exists(gameDir / L"version.dll"), "Newer installation left behind");
            });
            test("Manual older package blocks install of newer version", [&] {
                fs::copy_file(oldPayload, gameDir / L"winmm.dll"); manager.SelectRelease(Releases[0].tag);
                Require(Inspect(game).conflict, "Older manual installation ignored"); MustFail([&] { manager.Install(game); });
                fs::remove(gameDir / L"winmm.dll");
            });
            test("Missing older DLL receipt is recoverable with newer selection", [&] {
                manager.SelectRelease(Releases[1].tag); manager.Install(game); fs::remove(gameDir / L"version.dll");
                manager.SelectRelease(Releases[0].tag); Require(Inspect(game).recoverable, "Old recovery unavailable"); manager.Uninstall(game);
            });
            test("Unknown release cannot change selection or accept arbitrary URL", [&] {
                MustFail([&] { manager.SelectRelease("https://example.invalid/custom.dll"); });
                Require(&manager.SelectedRelease() == &Releases[0] && manager.PackageReady(), "Selection changed");
            });
        } else report << "SKIP  Cross-version tests: pass the official v1.3.3-hotfix.2 DLL as the third argument.\n";
        unsigned scenario = 0;
        auto presetTest = [&](const char* name, const std::function<void(FakeProfile&, const fs::path&)>& action) {
            test(name, [&] { FakeProfile driver; const auto state = root / (L"preset-" + std::to_wstring(++scenario)); action(driver, state); });
        };
        presetTest("Preset B sets BOTH driver values after durable backup", [&](auto& d, const auto& state) {
            const auto original = d.live.settings;
            d.beforeWrite = [&] { Require(fs::exists(PresetBackupPath(state, d.live.profile)), "Driver write preceded backup"); };
            SetPresetFix(game, state, d, true); auto inspected = InspectPresetFix(game, state, d);
            Require(inspected.managed && inspected.enabled && d.live.settings[0].value == 1 && d.live.settings[1].value == 2, "Both values not committed");
            Require(d.unrelated == 77, "Unrelated setting changed");
            SetPresetFix(game, state, d, false);
            Require(d.live.settings == original && !InspectPresetFix(game, state, d).managed, "Inheritance not restored");
        });
        presetTest("Existing custom NVIDIA values restored exactly", [&](auto& d, const auto& state) {
            d.live.settings = {{{true, true, 0}, {true, true, 1}}}; d.Reload(); const auto original = d.live.settings;
            SetPresetFix(game, state, d, true); SetPresetFix(game, state, d, false); Require(d.live.settings == original, "Custom values lost");
        });
        presetTest("Missing settings are removed instead of forced off on restore", [&](auto& d, const auto& state) {
            d.live.settings = {}; d.inherited = {}; d.Reload();
            SetPresetFix(game, state, d, true); SetPresetFix(game, state, d, false);
            Require(!d.live.settings[0].present && !d.live.settings[1].present, "Missing setting became forced value");
        });
        presetTest("Preset B alone still enables the companion override", [&](auto& d, const auto& state) {
            d.live.settings[1] = {true, true, 2}; d.Reload();
            Require(!InspectPresetFix(game, state, d).alreadySet, "Preset alone incorrectly counted as enabled");
            SetPresetFix(game, state, d, true); Require(d.live.settings[0].value == 1, "Enable flag missing");
        });
        presetTest("Already active driver workaround is not adopted", [&](auto& d, const auto& state) {
            d.live.settings = {{{true, true, 1}, {true, true, 2}}}; d.Reload();
            Require(InspectPresetFix(game, state, d).alreadySet, "Existing workaround missed");
            MustFail([&] { SetPresetFix(game, state, d, true); }); MustFail([&] { SetPresetFix(game, state, d, false); });
            Require(d.saves == 0 && !fs::exists(PresetBackupPath(state, d.live.profile)), "Unowned profile changed");
        });
        presetTest("Duplicate enable never replaces original driver backup", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); const auto path = PresetBackupPath(state, d.live.profile); const auto hash = Sha256(path);
            MustFail([&] { SetPresetFix(game, state, d, true); }); Require(Sha256(path) == hash, "Backup overwritten");
        });
        presetTest("Global NVIDIA profile can never be modified", [&](auto& d, const auto& state) {
            d.live.baseProfile = true; d.Reload(); MustFail([&] { SetPresetFix(game, state, d, true); }); Require(d.saves == 0, "Global profile saved");
        });
        presetTest("Profile shared with another game is rejected", [&](auto& d, const auto& state) {
            d.live.applications.push_back(L"OtherGame.exe"); d.Reload(); MustFail([&] { SetPresetFix(game, state, d, true); }); Require(d.saves == 0, "Other game affected");
        });
        presetTest("Cyberpunk profile with its registered launcher is supported", [&](auto& d, const auto& state) {
            Game cyber = game; cyber.exe = gameDir / L"Cyberpunk2077.exe"; Fixture(cyber.exe);
            d.live.profile = L"Cyberpunk 2077"; d.live.applications = {L"cyberpunk2077.exe", L"cplauncher.exe"}; d.Reload();
            SetPresetFix(cyber, state, d, true); SetPresetFix(cyber, state, d, false);
            Require(d.saves == 2, "Cyberpunk profile rejected");
        });
        presetTest("Driver backup from another install path cannot be reused", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); Game other = game; other.exe = root / L"TestGame.exe";
            MustFail([&] { SetPresetFix(other, state, d, false); }); Require(d.live.settings[1].value == 2, "Wrong installation restored");
        });
        presetTest("Changed NVIDIA setting blocks restore before either write", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); d.live.settings[1] = {true, true, 3}; d.Reload();
            Require(InspectPresetFix(game, state, d).conflict, "Conflict not shown");
            const auto saves = d.saves; MustFail([&] { SetPresetFix(game, state, d, false); });
            Require(d.saves == saves && d.live.settings[0].value == 1 && d.live.settings[1].value == 3, "External change overwritten");
        });
        presetTest("NVIDIA save failure preserves recovery without claiming success", [&](auto& d, const auto& state) {
            d.failSave = true; MustFail([&] { SetPresetFix(game, state, d, true); }); auto current = InspectPresetFix(game, state, d);
            Require(current.managed && !current.enabled && !current.conflict, "Failed save hidden");
            d.failSave = false; SetPresetFix(game, state, d, false); Require(!InspectPresetFix(game, state, d).managed, "Failed save not recoverable");
        });
        presetTest("Failure staging second driver setting does not persist first", [&](auto& d, const auto& state) {
            d.failStage = true; MustFail([&] { SetPresetFix(game, state, d, true); });
            Require(d.saves == 0 && d.live.settings[0].value == 0, "Half-staged value saved"); d.failStage = false; SetPresetFix(game, state, d, false);
        });
        presetTest("Partial NVIDIA save restores both values from journal", [&](auto& d, const auto& state) {
            d.partialSave = true; MustFail([&] { SetPresetFix(game, state, d, true); });
            Require(!InspectPresetFix(game, state, d).conflict, "Partial save not recoverable");
            d.partialSave = false; SetPresetFix(game, state, d, false); Require(!d.live.settings[0].local && !d.live.settings[1].local, "Partial save left modified setting");
        });
        presetTest("Driver success without actual persistence fails verification", [&](auto& d, const auto& state) {
            d.ignoreSave = true; MustFail([&] { SetPresetFix(game, state, d, true); }); Require(!InspectPresetFix(game, state, d).enabled, "False success reported");
        });
        presetTest("Failed restore retains backup for retry", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); d.failSave = true; MustFail([&] { SetPresetFix(game, state, d, false); });
            Require(fs::exists(PresetBackupPath(state, d.live.profile)), "Recovery backup lost"); d.failSave = false; SetPresetFix(game, state, d, false);
        });
        presetTest("Altered backup IDs cannot authorize unrelated driver setting", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); const auto path = PresetBackupPath(state, d.live.profile);
            auto doc = json::parse(ReadText(path)); doc["settings"][0]["id"] = 999; AtomicJson(path, doc);
            MustFail([&] { SetPresetFix(game, state, d, false); }); Require(d.saves == 1, "Untrusted setting written");
        });
        presetTest("Running game blocks profile changes before backup", [&](auto& d, const auto& state) {
            wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768); Game running = game; running.exe = executable;
            MustFail([&] { SetPresetFix(running, state, d, true); }); Require(d.saves == 0, "Running process profile changed");
        });
        presetTest("Profile restore works after game executable was removed", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); fs::remove(game.exe); SetPresetFix(game, state, d, false); Fixture(game.exe);
            Require(!InspectPresetFix(game, state, d).managed, "Missing game prevented restore");
        });
        presetTest("Changed inherited defaults remain inherited on restore", [&](auto& d, const auto& state) {
            SetPresetFix(game, state, d, true); d.inherited[1] = {true, false, 1}; SetPresetFix(game, state, d, false);
            Require(!d.live.settings[1].local && d.live.settings[1].value == 1, "Old inherited value was pinned");
        });
        unsigned neuralIndex=0;
        const auto progress=[](const std::wstring&){};
        auto neuralTest=[&](const char* name, const auto& run) {
            test(name,[&] {
                const auto dir=root/(L"neural-"+std::to_wstring(++neuralIndex)); fs::create_directories(dir);
                Game ng{L"Neural fixture",L"Test",dir,dir/L"TestGame.exe"}; Fixture(ng.exe,"dxgi.dll");
                const auto source=dir/L"source"; fs::create_directories(source);
                Bytes(source/L"new.dll","verified new neural fixture"); Bytes(source/L"new.ini","[GENERAL]\r\nPresetPath=test.ini\r\n");
                std::vector<NeuralFile> files{{source/L"new.dll",L"nvngx_dlssnr.dll",Sha256(source/L"new.dll")},
                    {source/L"new.ini",L"ReShade.ini",Sha256(source/L"new.ini"),true}};
                run(ng,files,dir/L".dlssalltools-dlss5"/L"manifest.json");
            });
        };
        neuralTest("DLSS5 replaces and restores originals while preserving MFG",[&](const auto& g,const auto& files,const auto&) {
            Bytes(g.root/L"nvngx_dlssnr.dll","original neural"); Bytes(g.root/L"ReShade.ini","original settings"); Bytes(g.root/L"version.dll","MFG unchanged");
            InstallNeuralFiles(g,files,{},progress); Require(ReadText(g.root/L"nvngx_dlssnr.dll")=="verified new neural fixture","Install failed");
            RestoreNeuralFiles(g,progress); Require(ReadText(g.root/L"nvngx_dlssnr.dll")=="original neural" && ReadText(g.root/L"ReShade.ini")=="original settings","Restore failed");
            Require(ReadText(g.root/L"version.dll")=="MFG unchanged","MFG was modified");
        });
        neuralTest("DLSS5 removes newly added files",[&](const auto& g,const auto& files,const auto&) {
            InstallNeuralFiles(g,files,{},progress); RestoreNeuralFiles(g,progress);
            Require(!fs::exists(g.root/L"nvngx_dlssnr.dll") && !fs::exists(g.root/L"ReShade.ini"),"Added files left behind");
        });
        neuralTest("DLSS5 shared identical files are not adopted or removed",[&](const auto& g,const auto& files,const auto&) {
            fs::copy_file(files[0].source,g.root/L"nvngx_dlssnr.dll"); InstallNeuralFiles(g,files,{},progress); RestoreNeuralFiles(g,progress);
            Require(fs::exists(g.root/L"nvngx_dlssnr.dll"),"Existing shared file removed");
        });
        neuralTest("DLSS5 refuses altered source before any game write",[&](const auto& g,const auto& files,const auto& manifest) {
            Bytes(files[0].source,"tampered"); MustFail([&]{InstallNeuralFiles(g,files,{},progress);});
            Require(!fs::exists(manifest) && !fs::exists(g.root/L"ReShade.ini"),"Changed before verifying");
        });
        neuralTest("DLSS5 refuses duplicate paths before creating journal",[&](const auto& g,auto files,const auto& manifest) {
            files.push_back(files.front()); MustFail([&]{InstallNeuralFiles(g,files,{},progress);}); Require(!fs::exists(manifest),"Invalid journal created");
        });
        neuralTest("DLSS5 external binary change blocks entire restore",[&](const auto& g,const auto& files,const auto&) {
            InstallNeuralFiles(g,files,{},progress); Bytes(g.root/L"nvngx_dlssnr.dll","someone else's update");
            MustFail([&]{RestoreNeuralFiles(g,progress);}); Require(fs::exists(g.root/L"ReShade.ini"),"Partial restore occurred");
        });
        neuralTest("DLSS5 changed game settings are archived before restore",[&](const auto& g,const auto& files,const auto&) {
            Bytes(g.root/L"ReShade.ini","original"); InstallNeuralFiles(g,files,{},progress); Bytes(g.root/L"ReShade.ini","changed during play");
            RestoreNeuralFiles(g,progress); Require(ReadText(g.root/L"ReShade.ini")=="original","Original not restored");
            bool found=false; for(const auto& f:fs::recursive_directory_iterator(g.root)) if(f.is_regular_file() && f.path().parent_path().filename()==L"son-ayarlar") found=ReadText(f.path())=="changed during play";
            Require(found,"Later settings lost");
        });
        neuralTest("DLSS5 corrupted original backup blocks restore",[&](const auto& g,const auto& files,const auto& manifest) {
            Bytes(g.root/L"nvngx_dlssnr.dll","old"); InstallNeuralFiles(g,files,{},progress); Bytes(manifest.parent_path()/L"0.before","corrupt");
            MustFail([&]{RestoreNeuralFiles(g,progress);}); Require(fs::exists(g.root/L"ReShade.ini"),"Changed despite damaged backup");
        });
        neuralTest("DLSS5 journal cannot escape selected game",[&](const auto& g,const auto& files,const auto& manifest) {
            InstallNeuralFiles(g,files,{},progress); auto j=json::parse(ReadText(manifest)); j["files"][0]["path"]="../outside.dll"; AtomicJson(manifest,j);
            MustFail([&]{RestoreNeuralFiles(g,progress);});
        });
        neuralTest("DLSS5 journal cannot target MFG proxy",[&](const auto& g,const auto& files,const auto& manifest) {
            InstallNeuralFiles(g,files,{},progress); auto j=json::parse(ReadText(manifest)); j["files"][0]["path"]="version.dll"; AtomicJson(manifest,j);
            MustFail([&]{RestoreNeuralFiles(g,progress);});
        });
        neuralTest("DLSS5 foreign game journal rejected",[&](const auto& g,const auto& files,const auto& manifest) {
            InstallNeuralFiles(g,files,{},progress); auto j=json::parse(ReadText(manifest)); j["exe"]="C:/elsewhere/game.exe"; AtomicJson(manifest,j);
            MustFail([&]{RestoreNeuralFiles(g,progress);});
        });
        neuralTest("DLSS5 interrupted copying restores partial deployment",[&](const auto& g,const auto& files,const auto& manifest) {
            Bytes(g.root/L"nvngx_dlssnr.dll","old"); unsigned count=0;
            MustFail([&]{InstallNeuralFiles(g,files,{},[&](const auto&){if(++count==2) throw std::runtime_error("power loss simulation");});});
            Require(fs::exists(manifest),"Recovery journal missing"); RestoreNeuralFiles(g,progress);
            Require(ReadText(g.root/L"nvngx_dlssnr.dll")=="old" && !fs::exists(g.root/L"ReShade.ini"),"Partial deployment not recovered");
        });
        neuralTest("DLSS5 missing original EXE still permits restore",[&](const auto& g,const auto& files,const auto&) {
            InstallNeuralFiles(g,files,{},progress); fs::remove(g.exe); RestoreNeuralFiles(g,progress); Require(!fs::exists(g.root/L"nvngx_dlssnr.dll"),"Missing exe blocked restore");
        });
        neuralTest("DLSS5 reinstall cannot overwrite first backup",[&](const auto& g,const auto& files,const auto&) {
            InstallNeuralFiles(g,files,{},progress); MustFail([&]{InstallNeuralFiles(g,files,{},progress);}); RestoreNeuralFiles(g,progress);
        });
        neuralTest("DLSS5 recovery from preparing phase preserves untouched game",[&](const auto& g,const auto& files,const auto& manifest) {
            InstallNeuralFiles(g,files,{},progress); fs::remove(g.root/L"nvngx_dlssnr.dll"); fs::remove(g.root/L"ReShade.ini");
            auto j=json::parse(ReadText(manifest)); j["state"]="preparing"; AtomicJson(manifest,j); RestoreNeuralFiles(g,progress);
            Require(!fs::exists(manifest),"Preparing journal not archived");
        });
        test("DLSS5 INI updates retain unrelated settings and sections",[&] {
            const auto text=NeuralIniSet("[GENERAL]\r\nPresetPath=old.ini\r\nKeep=123\r\n[INPUT]\r\nKey=36\r\n","GENERAL","PresetPath","new.ini");
            Require(text.find("PresetPath=new.ini")!=text.npos && text.find("Keep=123")!=text.npos && text.find("Key=36")!=text.npos,"INI preservation failed");
            const auto added=NeuralIniSet(text,"ADDON","AddonPath",".\\"); Require(added.find("[ADDON]\r\nAddonPath=.\\")!=added.npos,"Missing section not added");
        });
        test("GPU generation parser covers desktop laptop and Super",[&] {
            Require(ParseGpuSeries(L"NVIDIA GeForce RTX 2080 Ti")==GpuSeries::RTX20,"Turing detection");
            Require(ParseGpuSeries(L"NVIDIA GeForce RTX 2050 Laptop GPU")==GpuSeries::RTX20,"2050 detection");
            Require(ParseGpuSeries(L"NVIDIA GeForce RTX 3060 Laptop GPU")==GpuSeries::RTX30,"Ampere detection");
            Require(ParseGpuSeries(L"NVIDIA GeForce RTX 4070 SUPER")==GpuSeries::RTX40,"Ada detection");
            Require(ParseGpuSeries(L"NVIDIA GeForce RTX 5090")==GpuSeries::RTX50,"Blackwell detection");
        });
        test("Unsupported GPUs never fall back to RTX40",[&] {
            for(const auto* name:{L"NVIDIA GeForce GTX 1660",L"AMD Radeon RX 7900",L"Intel Arc",L"NVIDIA Quadro RTX 4000",L"NVIDIA RTX A5000",L"NVIDIA RTX PRO 5000",L"NVIDIA RTX 6000",L"RTX 40500",L""})Require(ParseGpuSeries(name)==GpuSeries::Unknown,"False supported GPU");
            MustFail([&]{GpuCode(GpuSeries::Unknown);});
            NeuralManager nm(root/L"unknown-gpu");Require(!nm.PackageReady(GpuSeries::Unknown),"Unknown marked ready");
            MustFail([&]{nm.Prepare(GpuSeries::Unknown,progress);});Require(!fs::exists(nm.Cache()),"Invalid selection wrote download cache");
        });
        test("Hardware gate refuses series absent from current machine",[&] {
            const auto actual=DetectedGpuSeries();Require(!MatchesHardware(GpuSeries::Unknown),"Unknown hardware accepted");MustFail([&]{RequireHardware(GpuSeries::Unknown);});
            for(const auto gpu:GpuChoices)if(IsSupportedGpu(gpu)){const bool present=std::find(actual.begin(),actual.end(),gpu)!=actual.end();Require(MatchesHardware(gpu)==present,"Series matching failed");if(!present)MustFail([&]{RequireHardware(gpu);});}
        });
        neuralTest("SM86 transaction restores config and leaves Neural files alone",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"version.dll";files[1].destination=L"dlssg_sm86.ini";Bytes(g.root/L"dxgi.dll","ReShade preserved");Bytes(g.root/L"nvngx_dlssnr.dll","NR preserved");
            InstallNeuralFiles(g,files,{{"gpu","20"}},progress,FileSet::Sm86);Bytes(g.root/L"dlssg_sm86.ini","user changed MFG settings");
            Require(InspectManagedFiles(g,FileSet::Sm86).installed,"SM86 receipt not recognized");RestoreNeuralFiles(g,progress,FileSet::Sm86);
            Require(!fs::exists(g.root/L"version.dll") && !fs::exists(g.root/L"dlssg_sm86.ini"),"SM86 removal incomplete");
            Require(ReadText(g.root/L"dxgi.dll")=="ReShade preserved" && ReadText(g.root/L"nvngx_dlssnr.dll")=="NR preserved","Neural files modified");
        });
        neuralTest("SM86 transaction cannot target Neural loader",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"dxgi.dll";files[1].destination=L"dlssg_sm86.ini";MustFail([&]{InstallNeuralFiles(g,files,{},progress,FileSet::Sm86);});Require(!fs::exists(g.root/Sm86Journal),"Illegal transaction wrote journal");
        });
        neuralTest("SM86 transaction interrupted copy can be undone",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"version.dll";files[1].destination=L"dlssg_sm86.ini";unsigned count=0;
            MustFail([&]{InstallNeuralFiles(g,files,{{"gpu","30"}},[&](const auto&){if(++count==2)throw std::runtime_error("interruption");},FileSet::Sm86);});
            Require(InspectManagedFiles(g,FileSet::Sm86).recoverable,"Interrupted SM86 not recoverable");RestoreNeuralFiles(g,progress,FileSet::Sm86);Require(!fs::exists(g.root/L"version.dll"),"Partial install left behind");
        });
        neuralTest("SM86 altered binary blocks all removals",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"version.dll";files[1].destination=L"dlssg_sm86.ini";InstallNeuralFiles(g,files,{{"gpu","30"}},progress,FileSet::Sm86);Bytes(g.root/L"version.dll","external mod");
            MustFail([&]{RestoreNeuralFiles(g,progress,FileSet::Sm86);});Require(fs::exists(g.root/L"dlssg_sm86.ini"),"Partial removal occurred");
        });
        neuralTest("SM86 forged journal path cannot remove Neural runtime",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"version.dll";files[1].destination=L"dlssg_sm86.ini";InstallNeuralFiles(g,files,{{"gpu","30"}},progress,FileSet::Sm86);
            const auto path=g.root/Sm86Journal/L"manifest.json";auto doc=json::parse(ReadText(path));doc["files"][0]["path"]="nvngx_dlssnr.dll";AtomicJson(path,doc);MustFail([&]{RestoreNeuralFiles(g,progress,FileSet::Sm86);});
        });
        neuralTest("Old RTX40 install cannot overlay managed SM86",[&](const auto& g,auto files,const auto&) {
            files[0].destination=L"version.dll";files[1].destination=L"dlssg_sm86.ini";InstallNeuralFiles(g,files,{{"gpu","20"}},progress,FileSet::Sm86);Require(Inspect(g).conflict,"RTX40 did not reject SM86");
        });
        report << "\n" << passed << " passed, " << failed << " failed.\n";
        report << "Only isolated test fixtures and a simulated NVIDIA store were modified. No real driver settings or games were changed.\n";
        report.flush();
        if (!failed) fs::remove_all(root);
    } catch (const std::exception& e) { report << "FATAL " << e.what() << '\n'; ++failed; }
    return failed ? 1 : 0;
}

int RunNeuralPackageTests(const fs::path& directory, const fs::path& state) {
    fs::create_directories(directory);
    const auto root=fs::absolute(directory)/(L"package-fixtures-"+std::to_wstring(GetCurrentProcessId()));
    std::ofstream report(directory/L"neural-package-tests.txt"); int passed=0,failed=0,index=0;
    NeuralManager manager(state); const auto progress=[](const std::wstring&){};
    auto test=[&](const char* name,const auto& action) {
        try {
            const auto dir=root/(L"Oyun Türkçe "+std::to_wstring(++index)); fs::create_directories(dir);
            Game game{L"Package fixture",L"Test",dir,dir/L"TestGame.exe"}; Fixture(game.exe,"dxgi.dll");
            action(game); report<<"PASS  "<<name<<'\n'; ++passed;
        } catch(const std::exception& e) { report<<"FAIL  "<<name<<": "<<e.what()<<'\n'; ++failed; }
        report.flush();
    };
    test("Pinned RTX40 package fully verified",[&](const auto&) { Require(manager.PackageReady(NeuralGpu::RTX40),"Package unavailable"); });
    if(manager.PackageReady(NeuralGpu::RTX40)) {
        test("Native actual binaries install and restore with MFG intact",[&](const auto& game) {
            Bytes(game.root/L"nvngx_dlss.dll","original SR"); Bytes(game.root/L"version.dll","MFG file preserved");
            Bytes(game.root/L"ReShade.ini","[INPUT]\r\nKeyOverlay=36,0,0,0\r\n");
            manager.Install(game,{},progress); Require(manager.Inspect(game).installed,"Native install not registered");
            Require(Sha256(game.root/L"nvngx_dlssnr.dll")=="4b8d19bc3eff58a084f5eca7489c921501c203450169fb82ff4f649a4482ba05","Wrong GPU runtime");
            Require(fs::exists(game.root/L"renodx-dlss5.addon64") && !fs::exists(game.root/L"dlss5-feed.addon64"),"Native route components incorrect");
            manager.Uninstall(game,progress); Require(ReadText(game.root/L"nvngx_dlss.dll")=="original SR" && ReadText(game.root/L"version.dll")=="MFG file preserved","Originals changed");
            Require(!fs::exists(game.root/L"dxgi.dll"),"Own ReShade not removed");
        });
        test("Existing add-on ReShade reused and remains after removal",[&](const auto& game) {
            fs::copy_file(manager.Cache()/L"reshade/dxgi.dll",game.root/L"dxgi.dll");
            const auto original=Sha256(game.root/L"dxgi.dll"); manager.Install(game,{},progress); manager.Uninstall(game,progress);
            Require(Sha256(game.root/L"dxgi.dll")==original,"Shared ReShade changed");
        });
        test("Unknown graphics proxy requires explicit option and round-trips",[&](const auto& game) {
            Bytes(game.root/L"dxgi.dll","existing graphics injector");
            MustFail([&]{manager.Install(game,{},progress);}); Require(!fs::exists(game.root/L".dlssalltools-dlss5"),"Rejected action wrote journal");
            NeuralOptions options; options.replaceProxy=true; manager.Install(game,options,progress); manager.Uninstall(game,progress);
            Require(ReadText(game.root/L"dxgi.dll")=="existing graphics injector","Proxy not restored");
        });
        test("Feeder actual package includes shaders and restores preset",[&](const auto& game) {
            Bytes(game.root/L"ReShade.ini","[GENERAL]\r\nPresetPath=OldPreset.ini\r\nEffectSearchPaths=.\\custom\r\n");
            Bytes(game.root/L"OldPreset.ini","user's visual settings");
            NeuralOptions options; options.route=NeuralRoute::Feeder; manager.Install(game,options,progress);
            Require(manager.Inspect(game).installed && fs::exists(game.root/L"dlss5-feed.addon64"),"Feeder not installed");
            Require(fs::exists(game.root/L"DLSSAllTools-shaders/Shaders/Includes/vort_MotionVectors.fxh"),"Shader dependency missing");
            Require(ReadText(game.root/L"ReShade.ini").find("EnableHooks=2")!=std::string::npos,"Consumer not configured");
            Require(ReadText(game.root/L"DLSSAllTools-DLSS5.ini").find("DLSS5_MV_PROVIDER=2")!=std::string::npos,"Motion provider missing");
            manager.Uninstall(game,progress); Require(ReadText(game.root/L"ReShade.ini").find("PresetPath=OldPreset.ini")!=std::string::npos,"Preset not restored");
            Require(ReadText(game.root/L"OldPreset.ini")=="user's visual settings","User preset changed");
        });
        test("Rival DLSS5 consumer rejected before writing",[&](const auto& game) {
            Bytes(game.root/L"renodx-dlss.addon64","other consumer"); MustFail([&]{manager.Install(game,{},progress);});
            Require(!fs::exists(game.root/L"nvngx_dlssnr.dll"),"Conflicting install wrote payload");
        });
        test("Unsupported renderer does not offer install",[&](const auto& game) {
            Fixture(game.exe,"opengl32.dll"); Require(!manager.Inspect(game).ready,"Unsupported API accepted"); MustFail([&]{manager.Install(game,{},progress);});
            Fixture(game.exe,"vulkan-1.dll"); Bytes(game.root/L"nvngx_dlss.dll","Vulkan DLSS"); Require(!manager.Inspect(game).ready,"Vulkan mistaken for DirectX because DLSS exists");
        });
    }
    report<<"\n"<<passed<<" passed, "<<failed<<" failed.\nReal packages copied as data only. No DLL loaded, game launched or real game directory changed.\n";
    report.close();
    // root is a generated, absolute child of the requested test output directory.
    if(!failed && fs::exists(root)) fs::remove_all(root);
    return failed?1:0;
}

int RunGpuPackageTests(const fs::path& directory,const fs::path& state) {
    fs::create_directories(directory);const auto root=fs::absolute(directory)/(L"gpu-fixtures-"+std::to_wstring(GetCurrentProcessId()));
    std::ofstream report(directory/L"gpu-package-tests.txt");int passed=0,failed=0,index=0;
    NeuralManager neural(state);Sm86Manager mfg(state);const auto progress=[](const std::wstring&){};
    auto test=[&](const char* name,const auto& action) {
        try{const auto dir=root/(L"Örnek Oyun "+std::to_wstring(++index));fs::create_directories(dir);Game game{L"GPU fixture",L"Test",dir,dir/L"TestGame.exe"};Fixture(game.exe,"version.dll");Bytes(dir/L"nvngx_dlssg.dll","FG evidence");Bytes(dir/L"nvngx_dlss.dll","SR original");action(game);report<<"PASS  "<<name<<'\n';++passed;}
        catch(const std::exception& e){report<<"FAIL  "<<name<<": "<<e.what()<<'\n';++failed;}report.flush();
    };
    test("All four GPU runtime packages verified",[&](const auto&){for(auto gpu:GpuChoices)if(IsSupportedGpu(gpu))Require(neural.PackageReady(gpu),"Missing GPU runtime");Require(mfg.PackageReady(),"MFG package not verified");});
    for(const auto gpu:{GpuSeries::RTX20,GpuSeries::RTX30}) {
        test(gpu==GpuSeries::RTX20?"RTX20 SF-v2 and MFG coexist, MFG removed first":"RTX30 SF-v2 and MFG coexist, Neural removed first",[&](const auto& game){
            mfg.Install(game,gpu,progress);NeuralOptions options;options.gpu=gpu;neural.Install(game,options,progress);
            Require(mfg.Inspect(game).installed && neural.Inspect(game).installed,"Independent receipts missing");
            Require(Sha256(game.root/L"nvngx_dlssnr.dll")=="6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927","Wrong GPU NR runtime");
            Require(Sha256(game.root/L"version.dll")==Sm86Files[0].hash,"Wrong MFG runtime");
            const auto ini=ReadText(game.root/L"dlssg_sm86.ini");Require(ini.find("MaxGeneratedFrames=3")!=ini.npos && ini.find("Optimized=1")!=ini.npos,"Factory 4x changed");
            if(gpu==GpuSeries::RTX20){mfg.Uninstall(game,progress);Require(neural.Inspect(game).installed,"MFG remove affected NR");neural.Uninstall(game,progress);}
            else{neural.Uninstall(game,progress);Require(mfg.Inspect(game).installed,"NR remove affected MFG");mfg.Uninstall(game,progress);}
            Require(ReadText(game.root/L"nvngx_dlss.dll")=="SR original" && ReadText(game.root/L"nvngx_dlssg.dll")=="FG evidence","Game DLSS originals changed");
            Require(!fs::exists(game.root/L"version.dll")&&!fs::exists(game.root/L"dxgi.dll")&&!fs::exists(game.root/L"nvngx_dlssnr.dll"),"Package files left behind");
        });
    }
    test("RTX50 original NR never resolves to RTX40 or SF-v2",[&](const auto& game){NeuralOptions options;options.gpu=GpuSeries::RTX50;neural.Install(game,options,progress);Require(Sha256(game.root/L"nvngx_dlssnr.dll")=="e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e","Wrong RTX50 NR");neural.Uninstall(game,progress);});
    test("SM86 refuses RTX40 and RTX50 choices before writing",[&](const auto& game){for(auto gpu:{GpuSeries::RTX40,GpuSeries::RTX50,GpuSeries::Unknown})MustFail([&]{mfg.Install(game,gpu,progress);});Require(!fs::exists(game.root/Sm86Journal),"Unsupported GPU wrote receipt");});
    test("SM86 requires existing FG and a utility load path",[&](const auto& game){fs::remove(game.root/L"nvngx_dlssg.dll");Require(!mfg.Inspect(game).fgEvidence,"No FG accepted");MustFail([&]{mfg.Install(game,GpuSeries::RTX20,progress);});Bytes(game.root/L"nvngx_dlssg.dll","FG evidence");Fixture(game.exe,"dxgi.dll");Require(mfg.Inspect(game).proxy.empty(),"DXGI hijacked");MustFail([&]{mfg.Install(game,GpuSeries::RTX30,progress);});});
    test("SM86 does not overwrite occupied utility loader",[&](const auto& game){Bytes(game.root/L"version.dll","other mod");MustFail([&]{mfg.Install(game,GpuSeries::RTX30,progress);});Require(ReadText(game.root/L"version.dll")=="other mod","Existing loader overwritten");});
    test("SM86 selects matching winmm binary without renaming version",[&](const auto& game){Fixture(game.exe,"winmm.dll");mfg.Install(game,GpuSeries::RTX30,progress);Require(Sha256(game.root/L"winmm.dll")==Sm86Files[1].hash && !fs::exists(game.root/L"version.dll"),"Wrong forwarding binary");mfg.Uninstall(game,progress);});
    test("SM86 manual configuration retained and install refused",[&](const auto& game){Bytes(game.root/L"dlssg_sm86.ini","existing user config");MustFail([&]{mfg.Install(game,GpuSeries::RTX20,progress);});Require(ReadText(game.root/L"dlssg_sm86.ini")=="existing user config","Config overwritten");});
    test("SM86 explicit Vulkan DX11 OpenGL rejected",[&](const auto& game){for(const auto* api:{"vulkan-1.dll","d3d11.dll","opengl32.dll"}){Fixture(game.exe,api);MustFail([&]{mfg.Install(game,GpuSeries::RTX30,progress);});}});
    test("SM86 missing game executable still allows restore",[&](const auto& game){mfg.Install(game,GpuSeries::RTX20,progress);fs::remove(game.exe);mfg.Uninstall(game,progress);Require(!fs::exists(game.root/L"version.dll"),"Restore requires missing EXE");});
    test("SM86 detects modified download and repairs pinned file",[&](const auto&){const auto temporary=state/L"test-sm86-tamper";Sm86Manager bad(temporary);fs::create_directories(bad.Cache());for(const auto& file:Sm86Files)fs::copy_file(mfg.Cache()/file.name,bad.Cache()/file.name,fs::copy_options::overwrite_existing);Bytes(bad.Cache()/L"dlssg_sm86.ini","corrupt cache");Require(!bad.PackageReady(),"Bad cache accepted");fs::copy_file(mfg.Cache()/L"dlssg_sm86.ini",bad.Cache()/L"dlssg_sm86.ini",fs::copy_options::overwrite_existing);Require(bad.PackageReady(),"Correct cache rejected");});
    report<<"\n"<<passed<<" passed, "<<failed<<" failed.\nReal pinned packages were copied as data into isolated fixtures. No GPU execution or game testing performed.\n";report.close();
    if(!failed&&fs::exists(root))fs::remove_all(root);return failed?1:0;
}
