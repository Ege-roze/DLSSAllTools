#include "neural.hpp"
#include <dwmapi.h>
#include <uxtheme.h>
#include <thread>
#include <memory>

namespace dlss {
namespace {
constexpr int DialogId=202, Gpu=1301, Route=1302, Replace=1303, Status=1304, Prepare=1305, Install=1306, Remove=1307, PackageInfo=1308;
constexpr UINT Complete = WM_APP + 21, ProgressText = WM_APP + 22;
struct Result { NeuralState state; std::wstring message; bool error=false; };
struct Dialog {
    Game game;
    GpuSeries initial;
    NeuralManager manager;
    HWND window=nullptr;
    bool busy=false;
    NeuralState state;
    std::thread worker;
    HBRUSH background=CreateSolidBrush(RGB(23,29,39));
    HFONT font=nullptr;
    Dialog(Game selected, fs::path dir, GpuSeries gpu) : game(std::move(selected)), initial(gpu), manager(std::move(dir)) {}
    ~Dialog() { if(worker.joinable()) worker.join(); DeleteObject(background); if(font)DeleteObject(font); }
    GpuSeries SelectedGpu() const {const auto index=SendDlgItemMessageW(window,Gpu,CB_GETCURSEL,0,0);return index>=0&&size_t(index)<std::size(GpuChoices)?GpuChoices[index]:GpuSeries::Unknown;}
    void PackageDetails() {
        auto info=NeuralPackageLabel(SelectedGpu());
        if(IsSupportedGpu(SelectedGpu())&&!MatchesHardware(SelectedGpu()))info+=Tr(L" Bu seri bilgisayarda yok: indirme açık, kurulum kapalı.");
        SetDlgItemTextW(window,PackageInfo,info.c_str());
    }
    void Controls() {
        for(int id : {Gpu, Route, Replace, Prepare, IDCANCEL}) EnableWindow(GetDlgItem(window,id), !busy);
        EnableWindow(GetDlgItem(window,Install), !busy && MatchesHardware(SelectedGpu()) && state.ready && !state.conflict && !state.installed && !state.recoverable);
        EnableWindow(GetDlgItem(window,Remove), !busy && !state.conflict && (state.installed || state.recoverable));
        EnableWindow(GetDlgItem(window,Prepare),!busy && IsSupportedGpu(SelectedGpu()));
        if(state.installed || state.recoverable) for(int id : {Gpu,Route,Replace}) EnableWindow(GetDlgItem(window,id),FALSE);
    }
    void Start(int action=0) {
        if(busy) return;
        if(worker.joinable()) worker.join();
        NeuralOptions options;
        options.gpu=SelectedGpu();
        options.route=SendDlgItemMessageW(window,Route,CB_GETCURSEL,0,0)==1 ? NeuralRoute::Feeder : NeuralRoute::Native;
        options.replaceProxy=IsDlgButtonChecked(window,Replace)==BST_CHECKED;
        busy=true; Controls(); SetDlgItemTextW(window,Status,Tr(L"DLSS 5 kurulumu kontrol ediliyor…"));
        worker=std::thread([this,options,action] {
            auto result=std::make_unique<Result>();
            const auto progress=[this](const std::wstring& value) {
                auto text=std::make_unique<std::wstring>(value);
                if(PostMessageW(window,ProgressText,0,LPARAM(text.get()))) text.release();
            };
            try {
                if(action==Prepare) { manager.Prepare(options.gpu,progress); result->message=Tr(L"DLSS 5 paketi hazır. Oyuna kur seçeneğini kullanabilirsiniz."); }
                else if(action==Install) { RequireHardware(options.gpu); manager.Install(game,options,progress); result->message=Tr(L"DLSS 5 kuruldu. Oyunda Home → Add-ons → RenoDX DLSS5. Mevcut DLSS yönteminde oyunun DLSS ayarını da açın."); }
                else if(action==Remove) { manager.Uninstall(game,progress); result->message=Tr(L"DLSS 5 kaldırıldı; önceki dosyalar geri alındı. Son ayarlar oyun klasöründeki .dlssalltools-dlss5-restored yedeğinde saklandı."); }
            } catch(const std::exception& e) { result->error=true; result->message=Wide(e.what()); }
            result->state=manager.Inspect(game);
            if(result->message.empty()) result->message=result->state.message;
            if(IsSupportedGpu(options.gpu))result->message+=manager.PackageReady(options.gpu)?Tr(L" Seçili paket hazır."):Tr(L" Seçili paket henüz indirilmedi.");
            if(PostMessageW(window,Complete,0,LPARAM(result.get()))) result.release();
        });
    }
};
INT_PTR CALLBACK Proc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    auto* dialog=reinterpret_cast<Dialog*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG) {
        dialog=reinterpret_cast<Dialog*>(lParam); dialog->window=window; SetWindowLongPtrW(window,DWLP_USER,LONG_PTR(dialog));
        dialog->font=TranslateDialog(window);
        BOOL dark=TRUE; DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
        SetWindowTextW(window,(dialog->game.name+L" — DLSS 5").c_str());
        for(int id : {Gpu,Route,Replace}) SetWindowTheme(GetDlgItem(window,id),L"DarkMode_Explorer",nullptr);
        const auto detected=dialog->initial;
        for(size_t i=0;i<std::size(GpuChoices);++i){const auto label=GpuLabel(GpuChoices[i]);SendDlgItemMessageW(window,Gpu,CB_ADDSTRING,0,LPARAM(label.c_str()));if(detected==GpuChoices[i])SendDlgItemMessageW(window,Gpu,CB_SETCURSEL,i,0);}
        dialog->PackageDetails();
        SendDlgItemMessageW(window,Route,CB_ADDSTRING,0,LPARAM(Tr(L"Oyunun mevcut DLSS özelliğini kullan")));
        SendDlgItemMessageW(window,Route,CB_ADDSTRING,0,LPARAM(Tr(L"DLSS olmayan oyun — Feeder")));
        const auto dir=dialog->game.exe.parent_path();
        SendDlgItemMessageW(window,Route,CB_SETCURSEL,fs::exists(dir/L"nvngx_dlss.dll") || fs::exists(dir/L"sl.interposer.dll") ? 0 : 1,0);
        dialog->Start(); return TRUE;
    }
    if(!dialog) return FALSE;
    switch(message) {
    case WM_CTLCOLORDLG: return INT_PTR(dialog->background);
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        SetTextColor(HDC(wParam),RGB(238,243,250)); SetBkColor(HDC(wParam),RGB(23,29,39)); return INT_PTR(dialog->background);
    case ProgressText: {
        std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(lParam)); SetDlgItemTextW(window,Status,text->c_str()); return TRUE;
    }
    case Complete: {
        std::unique_ptr<Result> result(reinterpret_cast<Result*>(lParam)); if(dialog->worker.joinable()) dialog->worker.join();
        dialog->busy=false; dialog->state=result->state; dialog->Controls();
        SetDlgItemTextW(window,Status,result->message.c_str()); return TRUE;
    }
    case WM_COMMAND:
        if(LOWORD(wParam)==Gpu && HIWORD(wParam)==CBN_SELCHANGE && !dialog->busy){dialog->PackageDetails();dialog->Start();return TRUE;}
        if(HIWORD(wParam)==BN_CLICKED && (LOWORD(wParam)==Prepare || LOWORD(wParam)==Install || LOWORD(wParam)==Remove)) { dialog->Start(LOWORD(wParam)); return TRUE; }
        if(LOWORD(wParam)==IDCANCEL && !dialog->busy) { EndDialog(window,0); return TRUE; }
        break;
    case WM_CLOSE: if(!dialog->busy) EndDialog(window,0); return TRUE;
    }
    return FALSE;
}
}
void ShowNeuralDialog(HWND owner,const Game& game,const fs::path& state,GpuSeries selected) {
    Dialog dialog(game,state,selected); DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(DialogId),owner,Proc,LPARAM(&dialog));
}
}
