#include "core.hpp"
#include "preset.hpp"
#include "neural.hpp"
#include "sm86.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <wrl/client.h>
#include <algorithm>
#include <memory>
#include <thread>

using namespace dlss;
int RunSelfTests(const fs::path& directory, const fs::path& payload, const fs::path& oldPayload);
int RunGpuPackageTests(const fs::path& directory,const fs::path& state);
int RunNeuralPackageTests(const fs::path& directory, const fs::path& state);
int RunLanguageTests(const fs::path& directory);

namespace {
constexpr UINT Finished = WM_APP + 1, ProgressMessage = WM_APP + 2;
enum Control { Scan = 101, Add, DownloadPackage, ImportPackage, Install, Uninstall, SelectExe, OpenFolder, OpenRelease, GameList, ReleaseSelect, Troubleshoot, Neural, GpuSelect, LanguageSelect };
constexpr COLORREF Background = RGB(15, 19, 26), Panel = RGB(23, 29, 39), Ink = RGB(238, 243, 250),
    Muted = RGB(150, 164, 184), Accent = RGB(173, 235, 103), Line = RGB(43, 53, 67), Danger = RGB(255, 175, 135);
constexpr int PresetDialogId = 201, PresetToggle = 1201, PresetStatus = 1202, PresetProfile = 1203;
constexpr UINT PresetFinished = WM_APP + 10;
struct PresetResult { PresetFixState state; bool available = false; std::wstring error; };
struct PresetDialog {
    Game game;
    fs::path directory;
    HWND window = nullptr;
    bool busy = false;
    PresetFixState current;
    std::thread worker;
    HBRUSH background = CreateSolidBrush(Panel);
    HFONT font = nullptr;
    ~PresetDialog() { if (worker.joinable()) worker.join(); DeleteObject(background); if(font)DeleteObject(font); }
    void Start(std::optional<bool> enable = std::nullopt) {
        if (busy) return;
        if (worker.joinable()) worker.join();
        busy = true; EnableWindow(GetDlgItem(window, PresetToggle), FALSE); EnableWindow(GetDlgItem(window, IDCANCEL), FALSE);
        SetDlgItemTextW(window, PresetStatus, enable ? Tr(L"Ayarlar yedekleniyor, uygulanıyor ve doğrulanıyor…") : Tr(L"NVIDIA oyun profili okunuyor…"));
        worker = std::thread([this, enable] {
            auto result = std::make_unique<PresetResult>();
            try {
                auto driver = OpenDriverProfile(game);
                if (enable) {
                    try { SetPresetFix(game, directory, *driver, *enable); }
                    catch (const std::exception& e) { result->error = Wide(e.what()); }
                }
                driver->Reload(); result->state = InspectPresetFix(game, directory, *driver); result->available = true;
                if (enable && !*enable && result->error.empty()) result->state.message = Tr(L"Önceki NVIDIA ayarları geri alındı. Oyunu yeniden açabilirsiniz.");
            } catch (const std::exception& e) { result->error = Wide(e.what()); }
            if (PostMessageW(window, PresetFinished, 0, LPARAM(result.get()))) result.release();
        });
    }
};
INT_PTR CALLBACK PresetProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* dialog = reinterpret_cast<PresetDialog*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        dialog = reinterpret_cast<PresetDialog*>(lParam); dialog->window = window;
        SetWindowLongPtrW(window, DWLP_USER, LONG_PTR(dialog));
        dialog->font = TranslateDialog(window);
        BOOL dark = TRUE; DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        SetWindowTheme(GetDlgItem(window, PresetToggle), L"DarkMode_Explorer", nullptr);
        SetWindowTextW(window, (dialog->game.name + Tr(L" — 3x / 4x görüntü sorunu")).c_str());
        dialog->Start(); return TRUE;
    }
    if (!dialog) return FALSE;
    switch (message) {
    case WM_CTLCOLORDLG: return INT_PTR(dialog->background);
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        SetTextColor(HDC(wParam), Ink); SetBkColor(HDC(wParam), Panel); return INT_PTR(dialog->background);
    case PresetFinished: {
        std::unique_ptr<PresetResult> result(reinterpret_cast<PresetResult*>(lParam));
        if (dialog->worker.joinable()) dialog->worker.join();
        dialog->busy = false; dialog->current = result->state;
        CheckDlgButton(window, PresetToggle, (result->state.managed || result->state.alreadySet) ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(window, PresetToggle), result->available && !result->state.conflict && !result->state.alreadySet);
        EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
        SetDlgItemTextW(window, PresetProfile, result->state.profile.empty() ? Tr(L"NVIDIA profili okunamadı") : (Tr(L"NVIDIA oyun profili: ") + result->state.profile).c_str());
        const auto text = result->error.empty() ? result->state.message : result->error + L"\n" + result->state.message;
        SetDlgItemTextW(window, PresetStatus, text.c_str()); return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == PresetToggle && HIWORD(wParam) == BN_CLICKED && !dialog->busy) { dialog->Start(!dialog->current.managed); return TRUE; }
        if (LOWORD(wParam) == IDCANCEL && !dialog->busy) { EndDialog(window, 0); return TRUE; }
        break;
    case WM_CLOSE:
        if (!dialog->busy) EndDialog(window, 0);
        return TRUE;
    }
    return FALSE;
}
struct WorkResult {
    std::wstring message;
    std::vector<Game> games;
    std::vector<Inspection> states;
    bool replaceGames = false, packageReady = false, error = false;
};
struct App {
    Manager manager;
    Sm86Manager sm86;
    GpuSeries series=DefaultGpuSeries();
    explicit App(fs::path state) : manager(state), sm86(std::move(state)) {}
    bool SelectedPackageReady() const {return IsOlderGpu(series)?sm86.PackageReady():series==GpuSeries::RTX40?manager.PackageReady():false;}
    Inspection InspectMfg(const Game& game) const {
        if(!game.exe.empty() && fs::exists(game.exe.parent_path()/Sm86Journal))return sm86.Inspect(game);
        if(!game.exe.empty() && fs::exists(game.exe.parent_path()/ReceiptName))return dlss::Inspect(game);
        if(IsOlderGpu(series))return sm86.Inspect(game);
        if(series==GpuSeries::RTX40)return dlss::Inspect(game);
        Inspection state;state.message=series==GpuSeries::RTX50?Tr(L"RTX 50, MFG'yi destekleyen oyunlarda doğrudan kullanır. Kilit açma modu gerekmez."):Tr(L"Önce ekran kartı serisini seçin.");return state;
    }
    void FillReleases() {
        SendMessageW(releaseSelect,CB_RESETCONTENT,0,0);
        if(series==GpuSeries::RTX40)for(size_t i=0;i<std::size(Releases);++i){SendMessageW(releaseSelect,CB_ADDSTRING,0,LPARAM(Tr(Releases[i].label)));if(&manager.SelectedRelease()==&Releases[i])SendMessageW(releaseSelect,CB_SETCURSEL,i,0);}
        else {SendMessageW(releaseSelect,CB_ADDSTRING,0,LPARAM(IsOlderGpu(series)?L"SM86 0.3.5 · 4x":series==GpuSeries::RTX50?Tr(L"Yerel MFG — mod gerekmez"):Tr(L"Önce seri seçin")));SendMessageW(releaseSelect,CB_SETCURSEL,0,0);}
    }
    HWND window = nullptr, list = nullptr, releaseSelect = nullptr, gpuSelect = nullptr, languageSelect = nullptr;
    std::vector<Game> games;
    std::vector<Inspection> states;
    std::wstring gpu, status = Tr(L"Oyun kütüphaneniz hazırlanıyor…");
    bool busy = false, packageReady = false, hasError = false;
    int selected = -1, width = 1120, height = 720;
    UINT dpi = 96;
    HFONT body = nullptr, captionFont = nullptr, title = nullptr, heading = nullptr;
    std::thread worker;
    ~App() { if (worker.joinable()) worker.join(); for (auto font : {body, captionFont, title, heading}) if (font) DeleteObject(font); }
    int S(int value) const { return MulDiv(value, int(dpi), 96); }
    void Fonts() {
        for (auto font : {body, captionFont, title, heading}) if (font) DeleteObject(font);
        auto make = [&](int size, int weight) { return CreateFontW(-S(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, UiFontName()); };
        body = make(14, FW_NORMAL); captionFont = make(12, FW_NORMAL); title = make(30, FW_SEMIBOLD); heading = make(18, FW_SEMIBOLD);
        if (list) SendMessageW(list, WM_SETFONT, WPARAM(body), TRUE);
        for(auto combo : {releaseSelect,gpuSelect,languageSelect}) if (combo) {
            SendMessageW(combo, WM_SETFONT, WPARAM(body), TRUE);
            SendMessageW(combo, CB_SETITEMHEIGHT, WPARAM(-1), S(30));
            SendMessageW(combo, CB_SETITEMHEIGHT, 0, S(30));
        }
    }
    HWND Button(int id, const wchar_t* label) {
        return CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, HMENU(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
    }
    void ApplyLanguage() {
        const std::pair<int,const wchar_t*> buttons[]{
            {Scan,L"Oyunları tara"},{Add,L"+  Oyun ekle"},{ImportPackage,L"DLL içe aktar"},
            {Install,L"MFG kur"},{Uninstall,L"MFG kaldır"},{Neural,L"DLSS 5 kur / kaldır…"},
            {SelectExe,L"EXE seç"},{OpenFolder,L"Klasörü aç"},{OpenRelease,L"Modun yayın sayfası ↗"},
            {Troubleshoot,L"3x / 4x görüntü sorunu…"}
        };
        for(const auto& [id,text]:buttons)SetWindowTextW(GetDlgItem(window,id),Tr(text));
        int columnIndex=0;
        for(const auto* text:{L"Oyun",L"Kaynak",L"MFG durumu"}) {
            LVCOLUMNW column{};column.mask=LVCF_TEXT;column.pszText=const_cast<wchar_t*>(Tr(text));
            ListView_SetColumn(list,columnIndex++,&column);
        }
        SendMessageW(gpuSelect,CB_RESETCONTENT,0,0);
        for(size_t i=0;i<std::size(GpuChoices);++i) {
            const auto label=GpuLabel(GpuChoices[i]);SendMessageW(gpuSelect,CB_ADDSTRING,0,LPARAM(label.c_str()));
            if(series==GpuChoices[i])SendMessageW(gpuSelect,CB_SETCURSEL,i,0);
        }
        SetWindowTextW(releaseSelect,Tr(L"Mod sürümü"));SetWindowTextW(gpuSelect,Tr(L"Ekran kartı serisi"));
        SetWindowTextW(languageSelect,Tr(L"Dil"));
        FillReleases();Fonts();gpu=GpuDescription();RefreshList();Layout();
    }
    void Layout() {
        RECT client{}; GetClientRect(window, &client); width = MulDiv(client.right, 96, dpi); height = MulDiv(client.bottom, 96, dpi);
        auto move = [&](int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(window, id), S(x), S(y), S(w), S(h), TRUE); };
        const int right = width - 356;
        move(LanguageSelect,width-195,26,167,180);
        move(Scan,28,126,120,38);move(Add,158,126,120,38);
        move(GpuSelect,290,126,175,260);move(ReleaseSelect,475,126,width-850,180);
        move(DownloadPackage,width-365,126,175,38);move(ImportPackage,width-180,126,152,38);
        move(GameList, 28, 218, right - 52, height - 324);
        ListView_SetColumnWidth(list, 0, S(std::max(180, right - 344)));
        ListView_SetColumnWidth(list, 1, S(104)); ListView_SetColumnWidth(list, 2, S(170));
        move(Install, right + 20, height - 317, 140, 39);
        move(Uninstall, right + 168, height - 317, 140, 39);
        move(Neural, right + 20, height - 268, 288, 43);
        move(SelectExe, right + 20, height - 207, 140, 35);
        move(OpenFolder, right + 168, height - 207, 140, 35);
        move(OpenRelease, right + 20, height - 154, 288, 30);
        move(Troubleshoot, right + 20, height - 108, 288, 30);
        InvalidateRect(window, nullptr, TRUE);
    }
    void Controls() {
        for (int id : {Scan, Add, DownloadPackage, ImportPackage, Install, Uninstall, SelectExe, OpenFolder, OpenRelease, ReleaseSelect, Troubleshoot, Neural, GpuSelect, LanguageSelect})
            EnableWindow(GetDlgItem(window, id), !busy);
        const bool chosen = selected >= 0 && size_t(selected) < games.size();
        const Inspection* state = chosen && size_t(selected) < states.size() ? &states[selected] : nullptr;
        EnableWindow(GetDlgItem(window, Install), !busy && MatchesHardware(series) && series!=GpuSeries::RTX50 && chosen && packageReady && state && !state->installed &&
            !state->conflict && !state->recoverable && state->fgEvidence && !state->proxy.empty());
        EnableWindow(GetDlgItem(window, Uninstall), !busy && state && !state->conflict && (state->installed || state->recoverable));
        EnableWindow(GetDlgItem(window, SelectExe), !busy && chosen && (!state || (!state->installed && !state->recoverable)));
        EnableWindow(GetDlgItem(window, OpenFolder), !busy && chosen);
        EnableWindow(GetDlgItem(window, Neural), !busy && chosen && !games[selected].exe.empty());
        EnableWindow(GetDlgItem(window, Troubleshoot), !busy && chosen && !games[selected].exe.empty());
        EnableWindow(releaseSelect,!busy && series==GpuSeries::RTX40);
        EnableWindow(GetDlgItem(window,ImportPackage),!busy && series==GpuSeries::RTX40);
        EnableWindow(GetDlgItem(window,DownloadPackage),!busy && (IsOlderGpu(series)||series==GpuSeries::RTX40));
        EnableWindow(GetDlgItem(window,Troubleshoot),!busy && chosen && series==GpuSeries::RTX40 && !games[selected].exe.empty());
        SetWindowTextW(GetDlgItem(window, DownloadPackage), series==GpuSeries::RTX50 ? Tr(L"Mod gerekmiyor") : packageReady ? Tr(L"MFG paketi hazır ✓") : Tr(L"MFG paketini indir"));
        InvalidateRect(window, nullptr, FALSE);
    }
    void RefreshList() {
        const int previous = selected;
        ListView_DeleteAllItems(list);
        for (size_t i = 0; i < games.size(); ++i) {
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = int(i); item.pszText = games[i].name.data();
            ListView_InsertItem(list, &item);
            ListView_SetItemText(list, int(i), 1, const_cast<wchar_t*>(Tr(games[i].store.c_str())));
            std::wstring label = Tr(L"EXE seçin");
            if (i < states.size()) {
                const auto& s = states[i];
                if (s.installed) label = Tr(L"Kurulu");
                else if (s.conflict) label = Tr(L"Kontrol gerekli");
                else if (s.recoverable) label = Tr(L"Eksik kurulum");
                else if (s.fgEvidence && !s.proxy.empty()) label = Tr(L"Kurulabilir*");
                else if (!games[i].exe.empty() && !s.fgEvidence) label = Tr(L"FG bulunamadı");
                else if (!games[i].exe.empty()) label = Tr(L"Profil gerekli");
            }
            ListView_SetItemText(list, int(i), 2, label.data());
        }
        selected = games.empty() ? -1 : std::clamp(previous, 0, int(games.size() - 1));
        if (selected >= 0) ListView_SetItemState(list, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        Controls();
    }
    template<class Action> void Start(Action action) {
        if (busy) return;
        if (worker.joinable()) worker.join();
        busy = true; hasError = false; Controls();
        const auto snapshot = games;
        worker = std::thread([this, action, snapshot]() {
            auto result = std::make_unique<WorkResult>();
            result->games = snapshot;
            try { action(*result); }
            catch (const std::exception& e) { result->error = true; try { result->message = Wide(e.what()); } catch (...) { result->message = Tr(L"İşlem tamamlanamadı."); } }
            result->packageReady = SelectedPackageReady();
            for (const auto& game : result->games) result->states.push_back(InspectMfg(game));
            if (PostMessageW(window, Finished, 0, LPARAM(result.get()))) result.release();
        });
    }
    void Say(const std::wstring& value) {
        auto text = std::make_unique<std::wstring>(value);
        if (PostMessageW(window, ProgressMessage, 0, LPARAM(text.get()))) text.release();
    }
    void ScanGames(bool initial) {
        status = Tr(L"Steam ve Epic Games kütüphaneleri taranıyor…");
        Start([this, initial](WorkResult& r) {
            if (initial) r.games = manager.LoadGames();
            auto found = DiscoverGames();
            for (auto& candidate : found.games) {
                auto it = std::find_if(r.games.begin(), r.games.end(), [&](const Game& g) { return Lower(g.root.wstring()) == Lower(candidate.root.wstring()); });
                if (it == r.games.end()) r.games.push_back(std::move(candidate));
                else if (it->exe.empty()) it->exe = candidate.exe;
            }
            manager.SaveGames(r.games); r.replaceGames = true;
            r.message = std::to_wstring(r.games.size()) + Tr(L" oyun listelendi. Kurmak için bir oyun seçin.");
            if (!found.warnings.empty()) r.message += L"  •  " + std::to_wstring(found.warnings.size()) + Tr(L" kayıt okunamadı; ayrıntılar tarama kaydında.");
            json notes = json::array(); for (const auto& warning : found.warnings) notes.push_back(Utf8(warning));
            AtomicJson(manager.State() / L"last-scan.json", {{"warnings", notes}, {"count", r.games.size()}});
        });
    }
    fs::path Pick(bool dll, const fs::path& initial = {}) {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
        const COMDLG_FILTERSPEC filter[] = {{dll ? L"RTXMFG DLL" : Tr(L"Oyun programı"), dll ? L"*.dll" : L"*.exe"}};
        dialog->SetFileTypes(1, filter); dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
        const auto prompt = dll ? Tr(L"Resmi ") + Wide(manager.SelectedRelease().tag) + Tr(L" RTXMFG.dll dosyasını seçin") : std::wstring(Tr(L"Oyunun gerçek EXE dosyasını seçin"));
        dialog->SetTitle(prompt.c_str());
        if (!initial.empty()) {
            Microsoft::WRL::ComPtr<IShellItem> folder;
            if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&folder)))) dialog->SetFolder(folder.Get());
        }
        if (FAILED(dialog->Show(window))) return {};
        Microsoft::WRL::ComPtr<IShellItem> item; dialog->GetResult(&item);
        PWSTR path = nullptr; if (!item || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return {};
        fs::path selectedPath(path); CoTaskMemFree(path); return selectedPath;
    }
    void Command(int id) {
        if (busy) return;
        if(id==LanguageSelect) {
            const auto index=SendMessageW(languageSelect,CB_GETCURSEL,0,0);
            if(index<0||size_t(index)>=std::size(Languages)||Languages[index]==CurrentLanguage())return;
            try {SaveLanguage(manager.State(),Languages[index]);}
            catch(...) {for(size_t i=0;i<std::size(Languages);++i)if(Languages[i]==CurrentLanguage())SendMessageW(languageSelect,CB_SETCURSEL,i,0);throw;}
            ApplyLanguage();status=Tr(L"Dil değiştirildi ve kaydedildi.");
            Start([](WorkResult& r){r.message=Tr(L"Dil değiştirildi ve kaydedildi.");});return;
        }
        if(id==GpuSelect) {
            const auto index=SendMessageW(gpuSelect,CB_GETCURSEL,0,0);
            if(index<0 || size_t(index)>=std::size(GpuChoices))return;
            series=GpuChoices[index];FillReleases();
            Start([this](WorkResult& r){
                r.message=IsOlderGpu(series)?GpuLabel(series)+Tr(L": MFG 0.3.5 / DLSS 5 SF-v2. DLSS 5 için sağdaki ayrı düğmeyi kullanın."):series==GpuSeries::RTX50?Tr(L"RTX 50: MFG için oyun ayarlarını kullanın. DLSS 5 paketi ayrı pencerede."):series==GpuSeries::RTX40?Tr(L"RTX 40 paketi seçildi. Çalışan mevcut sürüm tercihiniz korundu."):Tr(L"Önce ekran kartı serisini seçin.");
                if(IsSupportedGpu(series)&&!MatchesHardware(series))r.message+=Tr(L" Bu seri bilgisayarda bulunamadı: indirme açık, kurulum kapalı.");
            });return;
        }
        if (id == ReleaseSelect) {
            if(series!=GpuSeries::RTX40)return;
            const auto index = SendMessageW(releaseSelect, CB_GETCURSEL, 0, 0);
            if (index < 0 || size_t(index) >= std::size(Releases)) return;
            const std::string tag = Releases[index].tag;
            status = Tr(L"Seçilen mod paketi kontrol ediliyor…");
            Start([this, tag](WorkResult& r) {
                manager.SelectRelease(tag);
                r.message = Tr(L"Seçili paket: ") + Wide(tag) + Tr(L". Oyunda sürüm değiştirmek için önce Kaldır, ardından Kur seçin.");
            }); return;
        }
        if (id == Scan) { ScanGames(false); return; }
        if (id == OpenRelease) {
            const auto page = IsOlderGpu(series) ? std::wstring(L"https://github.com/sdli1995/dlssg_for_sm86/releases/tag/0.3.5") : L"https://github.com/dashdogy/RTX40MFG-Unlock/releases/tag/" + Wide(manager.SelectedRelease().tag);
            ShellExecuteW(window, L"open", page.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return;
        }
        if (id == DownloadPackage) {
            status = Tr(L"Mod paketi kontrol ediliyor…");
            if(!IsOlderGpu(series)&&series!=GpuSeries::RTX40)return;
            Start([this](WorkResult& r) {const auto progress=[this](const auto& text){Say(text);};if(IsOlderGpu(series))sm86.Prepare(progress);else manager.PreparePackage(progress);r.message=GpuLabel(series)+Tr(L" MFG paketi hazır. Tüm dosyalar doğrulandı.");if(!MatchesHardware(series))r.message+=Tr(L" Bu bilgisayara kurmak için algılanan seriyi seçin.");});return;
        }
        if (id == ImportPackage) {
            if(series!=GpuSeries::RTX40)return;
            const auto file = Pick(true); if (file.empty()) return;
            Start([this, file](WorkResult& r) { manager.ImportPackage(file); r.message = Tr(L"Mod paketi doğrulandı ve kaydedildi."); }); return;
        }
        if (id == Add || id == SelectExe) {
            if (id == SelectExe && selected < 0) return;
            const auto file = Pick(false, id == SelectExe ? games[selected].root : fs::path{}); if (file.empty()) return;
            if (!IsX64Image(file, false)) { MessageBoxW(window, Tr(L"64 bit bir oyun EXE dosyası seçin."), Tr(L"Oyun eklenemedi"), MB_OK | MB_ICONINFORMATION); return; }
            const int index = id == SelectExe ? selected : -1;
            Start([this, file, index](WorkResult& r) {
                if (index >= 0) r.games[size_t(index)].exe = file;
                else {
                    auto it = std::find_if(r.games.begin(), r.games.end(), [&](const auto& g) { return Lower(g.exe.wstring()) == Lower(file.wstring()); });
                    if (it == r.games.end()) r.games.push_back({file.stem().wstring(), L"Elle eklendi", file.parent_path(), file});
                }
                manager.SaveGames(r.games); r.replaceGames = true; r.message = Tr(L"Oyun konumu kaydedildi. Kurulum uygunluğu kontrol edildi.");
            }); return;
        }
        if (selected < 0 || size_t(selected) >= games.size()) return;
        const Game game = games[selected];
        if (id == Troubleshoot) {
            PresetDialog dialog; dialog.game = game; dialog.directory = manager.State();
            DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(PresetDialogId), window, PresetProc, LPARAM(&dialog)); return;
        }
        if (id == Neural) {
            ShowNeuralDialog(window, game, manager.State(),series);
            Start([](WorkResult& r) { r.message = Tr(L"MFG ve DLSS 5 ayrı ayrı yönetilir. DLSS 5 durumu kendi penceresinde gösterilir."); }); return;
        }
        if (id == OpenFolder) {
            const auto folder = game.exe.empty() ? game.root : game.exe.parent_path();
            ShellExecuteW(window, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return;
        }
        if (id == Install) {
            status = game.name + Tr(L" için mod kuruluyor…");
            Start([this, game](WorkResult& r) {RequireHardware(series);if(IsOlderGpu(series)){sm86.Install(game,series,[this](const auto& text){Say(text);});r.message=game.name+Tr(L": MFG 0.3.5 kuruldu. Oyunun DLSS Frame Generation ayarından 2x / 3x / 4x seçin.");}else if(series==GpuSeries::RTX40){manager.Install(game);r.message=game.name+L": "+Wide(manager.SelectedRelease().tag)+Tr(L" kuruldu. Mod menüsü: Backspace.");}else throw std::runtime_error(TrUtf8("Bu seri için MFG kilit açma paketi yok."));});
        } else if (id == Uninstall) {
            status = game.name + Tr(L" için mod kaldırılıyor…");
            Start([this, game](WorkResult& r) { if(fs::exists(game.exe.parent_path()/Sm86Journal))sm86.Uninstall(game,[this](const auto& text){Say(text);});else manager.Uninstall(game); r.message = game.name + Tr(L": mod kaldırıldı. Oyun ve mod ayarları korundu."); });
        }
    }
    void Text(HDC dc, const std::wstring& text, int x, int y, int w, int h, HFONT font, COLORREF color, UINT flags = DT_LEFT | DT_WORDBREAK | DT_NOPREFIX) {
        RECT rect{S(x), S(y), S(x + w), S(y + h)};
        SelectObject(dc, font); SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT); DrawTextW(dc, text.c_str(), -1, &rect, flags);
    }
    void Box(HDC dc, int x, int y, int w, int h, COLORREF color, int radius = 12) {
        auto brush = CreateSolidBrush(color); auto oldBrush = SelectObject(dc, brush); auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, S(x), S(y), S(x + w), S(y + h), S(radius), S(radius)); SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
    }
    void Paint() {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);
        auto background = CreateSolidBrush(Background); FillRect(dc, &client, background); DeleteObject(background);
        Text(dc, L"DLSS ALL TOOLS", 28, 24, 530, 44, title, Ink);
        Text(dc, Tr(L"Oyunlarını bul. Modunu kur. İstediğinde geri al."), 30, 72, 630, 28, body, Muted);
        Text(dc, L"MFG + DLSS 5  /  0.6", width - 460, 35, 240, 24, captionFont, Accent, DT_RIGHT | DT_SINGLELINE);
        Text(dc, L"Language / 语言", width - 193, 6, 167, 18, captionFont, Muted, DT_LEFT | DT_SINGLELINE);
        Text(dc, gpu, width - 430, 68, 400, 24, captionFont, Muted, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);
        Text(dc,Tr(L"EKRAN KARTI"),290,106,175,18,captionFont,Muted);
        Text(dc,Tr(L"MFG PAKETİ"),475,106,220,18,captionFont,Muted);
        const int right = width - 356;
        Text(dc, Tr(L"OYUN KÜTÜPHANESİ"), 30, 190, 260, 25, captionFont, Muted);
        Text(dc, std::to_wstring(games.size()) + Tr(L" oyun"), right - 142, 190, 116, 25, captionFont, Muted, DT_RIGHT | DT_SINGLELINE);
        Box(dc, right, 188, 328, height - 252, Panel);
        Text(dc, Tr(L"KURULUM"), right + 20, 207, 288, 22, captionFont, Accent);
        if (selected >= 0 && size_t(selected) < games.size()) {
            const auto& game = games[selected];
            Text(dc, game.name, right + 20, 238, 288, 52, heading, Ink);
            Text(dc, Tr(L"OYUN DOSYASI"), right + 20, 302, 288, 20, captionFont, Muted);
            Text(dc, game.exe.empty() ? Tr(L"EXE seçimi gerekiyor") : game.exe.filename().wstring(), right + 20, 325, 288, 22, body, Ink, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            Text(dc, game.exe.empty() ? game.root.wstring() : game.exe.parent_path().wstring(), right + 20, 351, 288, 20, captionFont, Muted, DT_LEFT | DT_SINGLELINE | DT_PATH_ELLIPSIS);
            if (size_t(selected) < states.size()) {
                const auto& state = states[selected];
                Text(dc, state.message + (state.installed ? Tr(L"\nDeğiştirmek için: Kaldır → Kur") : L""), right + 20, 389, 288, 60, body, state.conflict ? Danger : (state.installed ? Accent : Ink));
                if (!state.proxy.empty() && (state.fgEvidence || state.installed)) Text(dc, Tr(L"Kurulum dosyası: ") + state.proxy, right + 20, 454, 288, 24, captionFont, Muted);
            }
        } else {
            Text(dc, Tr(L"Bir oyun seçin"), right + 20, 245, 288, 34, heading, Ink);
            Text(dc, Tr(L"Steam ve Epic oyunlarınızı tarayın veya bir oyunu EXE dosyasından ekleyin."), right + 20, 294, 288, 90, body, Muted);
        }
        if (games.empty() && !busy) Text(dc, Tr(L"Henüz oyun yok\n\nOyunları tara veya Oyun ekle ile başlayın."), 70, 310, right - 140, 140, heading, Muted, DT_CENTER | DT_WORDBREAK);
        Text(dc, Tr(L"* Kurulabilir durumu dosya kontrolüdür; oyun içi MFG desteği garanti edilmez."), 30, height - 96, right - 54, 34, captionFont, Muted);
        Box(dc, 0, height - 52, width, 52, Panel, 0);
        Box(dc, 28, height - 30, 7, 7, hasError ? Danger : Accent, 7);
        Text(dc, status, 47, height - 39, width - 74, 34, captionFont, hasError ? Danger : Ink);
        EndPaint(window, &paint);
    }
    void DrawButton(const DRAWITEMSTRUCT* item) {
        if (item->CtlID == ReleaseSelect || item->CtlID == GpuSelect || item->CtlID == LanguageSelect) {
            auto brush = CreateSolidBrush(item->itemState & ODS_SELECTED ? Line : Panel);
            FillRect(item->hDC, &item->rcItem, brush); DeleteObject(brush);
            if(item->itemID!=UINT(-1)) {
                wchar_t choice[256]{};if(SendMessageW(item->hwndItem,CB_GETLBTEXTLEN,item->itemID,0)>=256)return;SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,LPARAM(choice));
                auto rect = item->rcItem; rect.left += S(8);
                SelectObject(item->hDC, body); SetBkMode(item->hDC, TRANSPARENT);
                SetTextColor(item->hDC, item->itemState & ODS_DISABLED ? Muted : Ink);
                DrawTextW(item->hDC, choice, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
            }
            return;
        }
        wchar_t label[128]{}; GetWindowTextW(item->hwndItem, label, 128);
        const bool disabled = item->itemState & ODS_DISABLED, pressed = item->itemState & ODS_SELECTED;
        const bool primary = item->CtlID == Install || item->CtlID == DownloadPackage;
        COLORREF color = disabled ? RGB(35, 43, 53) : primary ? (pressed ? RGB(137, 196, 72) : Accent) : (pressed ? RGB(58, 72, 90) : RGB(39, 49, 64));
        HDC dc = item->hDC;
        auto parentBrush = CreateSolidBrush(item->CtlID >= Install ? Panel : Background);
        FillRect(dc, &item->rcItem, parentBrush); DeleteObject(parentBrush);
        auto brush = CreateSolidBrush(color); auto oldBrush = SelectObject(dc, brush); auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom, S(9), S(9));
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(brush);
        SelectObject(dc, body); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, disabled ? RGB(108, 121, 137) : primary ? Background : Ink);
        RECT rect = item->rcItem; DrawTextW(dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT)) { InflateRect(&rect, -4, -4); DrawFocusRect(dc, &rect); }
    }
};

LRESULT CALLBACK ListSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    auto app = reinterpret_cast<App*>(data);
    if (message == WM_NOTIFY && reinterpret_cast<NMHDR*>(lParam)->code == NM_CUSTOMDRAW) {
        auto draw = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
        if (draw->hdr.hwndFrom == ListView_GetHeader(window)) {
            if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                auto brush = CreateSolidBrush(Panel); FillRect(draw->hdc, &draw->rc, brush); DeleteObject(brush);
                wchar_t label[128]{}; HDITEMW item{}; item.mask = HDI_TEXT; item.pszText = label; item.cchTextMax = 128;
                Header_GetItem(draw->hdr.hwndFrom, int(draw->dwItemSpec), &item);
                SelectObject(draw->hdc, app->captionFont); SetBkMode(draw->hdc, TRANSPARENT); SetTextColor(draw->hdc, Muted);
                auto rect = draw->rc; rect.left += app->S(6); DrawTextW(draw->hdc, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return CDRF_SKIPDEFAULT;
            }
        }
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->window = window; SetWindowLongPtrW(window, GWLP_USERDATA, LONG_PTR(app));
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    try {
        switch (message) {
        case WM_CREATE: {
            app->dpi = GetDpiForWindow(window); app->Fonts();
            BOOL dark = TRUE; DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
            app->Button(Scan, Tr(L"Oyunları tara")); app->Button(Add, Tr(L"+  Oyun ekle"));
            app->releaseSelect = CreateWindowExW(0, L"COMBOBOX", Tr(L"Mod sürümü"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, window, HMENU(ReleaseSelect), GetModuleHandleW(nullptr), nullptr);
            SetWindowTheme(app->releaseSelect, L"DarkMode_CFD", nullptr);
            app->gpuSelect=CreateWindowExW(0,L"COMBOBOX",Tr(L"Ekran kartı serisi"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,0,0,0,0,window,HMENU(GpuSelect),GetModuleHandleW(nullptr),nullptr);
            SetWindowTheme(app->gpuSelect,L"DarkMode_CFD",nullptr);
            app->languageSelect=CreateWindowExW(0,L"COMBOBOX",Tr(L"Dil"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,0,0,0,0,window,HMENU(LanguageSelect),GetModuleHandleW(nullptr),nullptr);
            SetWindowTheme(app->languageSelect,L"DarkMode_CFD",nullptr);
            for(size_t i=0;i<std::size(Languages);++i){SendMessageW(app->languageSelect,CB_ADDSTRING,0,LPARAM(LanguageName(Languages[i])));if(CurrentLanguage()==Languages[i])SendMessageW(app->languageSelect,CB_SETCURSEL,i,0);}
            for(size_t i=0;i<std::size(GpuChoices);++i){const auto label=GpuLabel(GpuChoices[i]);SendMessageW(app->gpuSelect,CB_ADDSTRING,0,LPARAM(label.c_str()));if(app->series==GpuChoices[i])SendMessageW(app->gpuSelect,CB_SETCURSEL,i,0);}
            app->FillReleases();
            app->Fonts();
            app->Button(DownloadPackage, Tr(L"Mod paketini indir")); app->Button(ImportPackage, Tr(L"DLL içe aktar"));
            app->Button(Install, Tr(L"MFG kur")); app->Button(Uninstall, Tr(L"MFG kaldır"));
            app->Button(Neural, Tr(L"DLSS 5 kur / kaldır…"));
            app->Button(SelectExe, Tr(L"EXE seç")); app->Button(OpenFolder, Tr(L"Klasörü aç")); app->Button(OpenRelease, Tr(L"Modun yayın sayfası ↗"));
            app->Button(Troubleshoot, Tr(L"3x / 4x görüntü sorunu…"));
            app->list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                0, 0, 0, 0, window, HMENU(GameList), GetModuleHandleW(nullptr), nullptr);
            ListView_SetExtendedListViewStyle(app->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
            ListView_SetBkColor(app->list, Background); ListView_SetTextBkColor(app->list, Background); ListView_SetTextColor(app->list, Ink);
            SetWindowTheme(app->list, L"DarkMode_Explorer", nullptr);
            SetWindowSubclass(app->list, ListSubclass, 1, DWORD_PTR(app));
            SendMessageW(app->list, WM_SETFONT, WPARAM(app->body), TRUE);
            int columnIndex = 0;
            for (const auto* text : {Tr(L"Oyun"), Tr(L"Kaynak"), Tr(L"MFG durumu")}) {
                LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH; column.pszText = const_cast<wchar_t*>(text); column.cx = app->S(180);
                ListView_InsertColumn(app->list, columnIndex++, &column);
            }
            app->gpu = GpuDescription(); app->Layout(); app->ScanGames(true); return 0;
        }
        case WM_SIZE: app->Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam); info->ptMinTrackSize = {app->S(1000), app->S(840)}; return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(wParam); app->Fonts(); auto rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER); app->Layout(); return 0;
        }
        case WM_PAINT: app->Paint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_DRAWITEM: app->DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE;
        case WM_COMMAND:
            if (((LOWORD(wParam)==ReleaseSelect||LOWORD(wParam)==GpuSelect||LOWORD(wParam)==LanguageSelect) && HIWORD(wParam)==CBN_SELCHANGE) ||
                (LOWORD(wParam)!=ReleaseSelect && LOWORD(wParam)!=GpuSelect && LOWORD(wParam)!=LanguageSelect && HIWORD(wParam)==BN_CLICKED)) app->Command(LOWORD(wParam));
            return 0;
        case WM_NOTIFY: {
            auto header = reinterpret_cast<NMHDR*>(lParam);
            if (header->idFrom == GameList && header->code == LVN_ITEMCHANGED) {
                app->selected = ListView_GetNextItem(app->list, -1, LVNI_SELECTED); app->Controls();
            }
            return 0;
        }
        case ProgressMessage: {
            std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(lParam)); app->status = *text; InvalidateRect(window, nullptr, FALSE); return 0;
        }
        case Finished: {
            std::unique_ptr<WorkResult> result(reinterpret_cast<WorkResult*>(lParam));
            if (app->worker.joinable()) app->worker.join();
            app->busy = false; app->hasError = result->error; app->status = result->message;
            app->packageReady = result->packageReady; app->games = std::move(result->games); app->states = std::move(result->states);
            app->FillReleases();
            app->RefreshList(); return 0;
        }
        case WM_CLOSE:
            if (app->busy) { app->status = Tr(L"İşlem sürüyor. Tamamlandığında pencereyi kapatabilirsiniz."); InvalidateRect(window, nullptr, FALSE); return 0; }
            DestroyWindow(window); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
    } catch (const std::exception& e) {
        app->status = Wide(e.what()); app->hasError = true; InvalidateRect(window, nullptr, FALSE);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int count = 0; auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    if(args && count==3 && std::wstring(args[1])==L"--language-tests") {const int code=RunLanguageTests(args[2]);LocalFree(args);if(SUCCEEDED(com))CoUninitialize();return code;}
    if(args && count==4 && std::wstring(args[1])==L"--gpu-package-tests") {const int code=RunGpuPackageTests(args[2],args[3]);LocalFree(args);if(SUCCEEDED(com))CoUninitialize();return code;}
    if (args && count == 4 && std::wstring(args[1]) == L"--neural-package-tests") {
        const int code=RunNeuralPackageTests(args[2],args[3]); LocalFree(args); if(SUCCEEDED(com)) CoUninitialize(); return code;
    }
    if (args && count == 4 && std::wstring(args[1]) == L"--prepare-neural") {
        int code = 0;
        try {
            const std::wstring gpu(args[3]); if(gpu!=L"20" && gpu!=L"30" && gpu!=L"40" && gpu!=L"50")throw std::runtime_error(TrUtf8("GPU paketi 20, 30, 40 veya 50 olmalı."));
            NeuralManager manager(args[2]); const auto selected=static_cast<GpuSeries>(std::stoi(gpu));
            manager.Prepare(selected,[](const auto&){});
            AtomicJson(fs::path(args[2])/L"neural-download-result.json",{{"ready",manager.PackageReady(selected)}, {"gpu",Utf8(gpu)}});
        } catch(const std::exception& e) { code=1; try { AtomicJson(fs::path(args[2])/L"neural-download-result.json",{{"ready",false},{"error",e.what()}}); } catch(...) {} }
        LocalFree(args); if(SUCCEEDED(com)) CoUninitialize(); return code;
    }
    if(args && count==3 && std::wstring(args[1])==L"--prepare-sm86") {
        int code=0;try{Sm86Manager manager(args[2]);manager.Prepare([](const auto&){});AtomicJson(fs::path(args[2])/L"sm86-download-result.json",{{"ready",manager.PackageReady()}});}catch(const std::exception& e){code=1;try{AtomicJson(fs::path(args[2])/L"sm86-download-result.json",{{"ready",false},{"error",e.what()}});}catch(...){}}
        LocalFree(args);if(SUCCEEDED(com))CoUninitialize();return code;
    }
    if (args && count == 4 && std::wstring(args[1]) == L"--driver-profile-report") {
        int code = 0;
        try {
            fs::path exe(args[2]); Game game{exe.stem().wstring(), Tr(L"Tanı"), exe.parent_path(), exe};
            AtomicJson(args[3], DriverReport(game));
        } catch (const std::exception& e) { code = 1; try { AtomicJson(args[3], {{"error", e.what()}}); } catch (...) {} }
        LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return code;
    }
    if (args && (count == 4 || count == 5) && std::wstring(args[1]) == L"--self-test") {
        const auto result = RunSelfTests(args[2], args[3], count == 5 ? fs::path(args[4]) : fs::path{}); LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return result;
    }
    if (args && count == 3 && std::wstring(args[1]) == L"--scan-report") {
        int code = 0;
        try {
            auto found = DiscoverGames(); json games = json::array();
            for (const auto& game : found.games) {
                auto state = Inspect(game);
                games.push_back({{"name", Utf8(game.name)}, {"store", Utf8(game.store)}, {"exe", Utf8(game.exe.wstring())},
                    {"status", Utf8(state.message)}, {"proxy", Utf8(state.proxy)}, {"fgEvidence", state.fgEvidence}});
            }
            AtomicJson(args[2], {{"games", games}, {"gpu", Utf8(GpuDescription())}});
        } catch (...) { code = 1; }
        LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return code;
    }
    if (args && (count == 3 || count == 4) && std::wstring(args[1]) == L"--prepare-package") {
        int code = 0;
        try {
            Manager manager(args[2], count == 4 ? Utf8(args[3]) : ""); manager.PreparePackage([](const auto&) {});
            if (count == 4) manager.SelectRelease(Utf8(args[3]));
            AtomicJson(manager.State() / L"download-result.json", {{"ready", manager.PackageReady()}, {"version", manager.SelectedRelease().tag}, {"sha256", Sha256(manager.Payload())}});
        } catch (const std::exception& e) {
            code = 1;
            try { AtomicJson(fs::path(args[2]) / L"download-result.json", {{"ready", false}, {"error", e.what()}}); } catch (...) {}
        }
        LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return code;
    }
    // Two managers must not mutate the same installation concurrently.
    HANDLE singleton = CreateMutexW(nullptr, TRUE, L"Local\\DLSSAllTools-Desktop-v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (auto existing = FindWindowW(L"DLSSAllToolsWindow", nullptr)) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
        if (singleton) CloseHandle(singleton); if (args) LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return 0;
    }
    int result = 0;
    try {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
        // Optional isolated state folder is for UI QA; no change to the user's normal library.
        const auto state=args && count == 3 && std::wstring(args[1]) == L"--state-dir" ? fs::path(args[2]) : StateDirectory();
        LoadLanguage(state);
        App app(state);
        WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.lpszClassName = L"DLSSAllToolsWindow";
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101)); cls.hIconSm = cls.hIcon;
        RegisterClassExW(&cls);
        const UINT dpi = GetDpiForSystem();
        HWND window = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"DLSS All Tools", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(1160, dpi, 96), MulDiv(860, dpi, 96), nullptr, nullptr, instance, &app);
        if (!window) throw std::runtime_error(TrUtf8("Uygulama penceresi oluşturulamadı."));
        ShowWindow(window, show); UpdateWindow(window);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        result = int(message.wParam);
    } catch (const std::exception& e) { MessageBoxW(nullptr, Wide(e.what()).c_str(), L"DLSS All Tools", MB_OK | MB_ICONERROR); result = 1; }
    if (singleton) { ReleaseMutex(singleton); CloseHandle(singleton); }
    if (args) LocalFree(args); if (SUCCEEDED(com)) CoUninitialize(); return result;
}


