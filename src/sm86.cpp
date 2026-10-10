#include "sm86.hpp"
#include <algorithm>
namespace dlss {
fs::path Sm86Manager::Cache() const {return state_/L"packages"/L"sm86-0.3.5-310.9";}
bool Sm86Manager::PackageReady() const {
    try {for(const auto& file:Sm86Files) {const auto path=Cache()/file.name;RejectLinkedPath(path);if(!fs::is_regular_file(path)||Sha256(path)!=file.hash)return false;}return true;}
    catch(...) {return false;}
}
void Sm86Manager::Prepare(const Progress& progress) const {
    RejectLinkedPath(Cache());fs::create_directories(Cache());
    for(const auto& file:Sm86Files) {
        progress(Tr(L"RTX 20/30 MFG 0.3.5 hazırlanıyor: ")+std::wstring(file.name));
        DownloadVerified(L"https://raw.githubusercontent.com/sdli1995/dlssg_for_sm86/9621db573e07ed54f50c15bbb585ed9a7bdfac28/"+std::wstring(file.upstream),Cache()/file.name,file.hash,progress);
        if(std::wstring(file.name).ends_with(L".dll")&&!IsX64Image(Cache()/file.name,true)) throw std::runtime_error(TrUtf8("MFG paketi 64 bit DLL değil."));
    }
    if(!PackageReady())throw std::runtime_error(TrUtf8("MFG dosyaları doğrulanamadı."));
}
Inspection Sm86Manager::Inspect(const Game& game) const {
    Inspection result;
    try {
        auto managed=InspectManagedFiles(game,FileSet::Sm86);
        if(managed.installed||managed.recoverable||managed.conflict) {
            result.installed=managed.installed;result.recoverable=managed.recoverable;result.conflict=managed.conflict;result.message=managed.message;return result;
        }
        // The older manager supplies bounded FG discovery and existing-mod detection.
        result=dlss::Inspect(game);
        if(result.installed||result.recoverable) {result.installed=false;result.recoverable=false;result.conflict=true;result.message=Tr(L"Önce mevcut RTX 40 MFG kurulumunu kaldırın.");return result;}
        if(result.conflict||game.exe.empty()||!result.fgEvidence)return result;
        result.proxy.clear();
        const auto dir=game.exe.parent_path();
        if(fs::exists(dir/L"dlssg_sm86.ini")) {result.conflict=true;result.message=Tr(L"Elle kurulmuş RTX 20/30 MFG ayarı bulundu. Önce mevcut modu kaldırın.");return result;}
        const auto imports=PeImports(game.exe);
        const auto has=[&](const char* name){return std::find(imports.begin(),imports.end(),name)!=imports.end();};
        if(!has("d3d12.dll")&&(has("vulkan-1.dll")||has("opengl32.dll")||has("d3d11.dll")||has("d3d9.dll"))) {
            result.fgEvidence=false;result.message=Tr(L"RTX 20/30 MFG için oyunun DirectX 12 EXE dosyasını seçin.");return result;
        }
        for(const auto& file:Sm86Files) {
            const std::wstring name=file.name;if(!name.ends_with(L".dll"))continue;
            const auto dest=dir/name;RejectLinkedPath(dest);
            if(fs::exists(dest)) {
                const auto hash=Sha256(dest);
                for(const auto& known:Sm86Files)if(hash==known.hash){result.conflict=true;result.message=Tr(L"Elle kurulmuş MFG 0.3.5 bulundu. Dosya korunuyor.");return result;}
                continue;
            }
            if(result.proxy.empty()&&has(Utf8(name).c_str()))result.proxy=name;
        }
        if(result.proxy.empty()&&Lower(game.exe.filename().wstring())==L"witcher3.exe"&&Lower(dir.filename().wstring())==L"x64_dx12"&&!fs::exists(dir/L"winmm.dll"))result.proxy=L"winmm.dll";
        result.message=result.proxy.empty()?Tr(L"Boş ve doğrulanabilir bir yükleme yolu yok. Oyun profili gerekiyor."):Tr(L"MFG 0.3.5 · DX12 · 2x/3x/4x. Oyuna göre uyumluluk değişir.");
    }catch(const std::exception& e){result.conflict=true;result.message=Wide(e.what());}
    return result;
}
void Sm86Manager::Install(const Game& game,GpuSeries gpu,const Progress& progress) const {
    if(!IsOlderGpu(gpu))throw std::runtime_error(TrUtf8("Bu MFG paketi yalnızca RTX 20 ve RTX 30 için seçilebilir."));
    if(GameRunning(game.exe))throw std::runtime_error(TrUtf8("Önce oyunu tamamen kapatın."));
    const auto check=Inspect(game);
    if(check.installed||check.recoverable||check.conflict||!check.fgEvidence||check.proxy.empty())throw std::runtime_error(Utf8(check.message));
    Prepare(progress);
    // Re-check after downloading; do not overwrite an intervening external install.
    const auto again=Inspect(game);
    if(again.conflict||again.installed||again.recoverable||again.proxy!=check.proxy)throw std::runtime_error(TrUtf8("Oyun dosyaları değişti; yeniden kontrol edin."));
    std::vector<NeuralFile> files;
    for(const auto& file:Sm86Files)if(check.proxy==file.name || std::wstring(file.name)==L"dlssg_sm86.ini")files.push_back({Cache()/file.name,file.name,file.hash,std::wstring(file.name)==L"dlssg_sm86.ini"});
    InstallNeuralFiles(game,files,{{"gpu",GpuCode(gpu)},{"release","0.3.5"},{"runtime","310.9"},{"proxy",Utf8(check.proxy)}},progress,FileSet::Sm86);
}
void Sm86Manager::Uninstall(const Game& game,const Progress& progress) const {RestoreNeuralFiles(game,progress,FileSet::Sm86);}
}
