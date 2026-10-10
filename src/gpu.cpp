#include "gpu.hpp"
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <regex>
namespace dlss {
GpuSeries ParseGpuSeries(const std::wstring& name) {
    // Do not mistake Quadro RTX 3000/4000, RTX A-series or RTX PRO for GeForce generations.
    const auto lower=Lower(name);
    if(lower.find(L"quadro")!=lower.npos || lower.find(L"rtx a")!=lower.npos || lower.find(L"rtx pro")!=lower.npos) return GpuSeries::Unknown;
    static const std::wregex model(LR"(\brtx\s*(20|30|40|50)([5-9][0-9])\b)");
    std::wsmatch match;
    if(!std::regex_search(lower,match,model)) return GpuSeries::Unknown;
    return static_cast<GpuSeries>(std::stoi(match[1]));
}
bool IsSupportedGpu(GpuSeries gpu) { return gpu==GpuSeries::RTX20 || gpu==GpuSeries::RTX30 || gpu==GpuSeries::RTX40 || gpu==GpuSeries::RTX50; }
bool IsOlderGpu(GpuSeries gpu) { return gpu==GpuSeries::RTX20 || gpu==GpuSeries::RTX30; }
std::string GpuCode(GpuSeries gpu) { if(!IsSupportedGpu(gpu)) throw std::runtime_error(TrUtf8("Önce RTX 20, 30, 40 veya 50 serisini seçin.")); return std::to_string(int(gpu)); }
std::wstring GpuLabel(GpuSeries gpu) { return IsSupportedGpu(gpu) ? L"RTX "+std::to_wstring(int(gpu))+Tr(L" serisi") : Tr(L"Ekran kartı serisini seçin"); }
std::vector<GpuSeries> DetectedGpuSeries() {
    std::vector<GpuSeries> result;
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return result;
    for(UINT i=0;;++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND) break;
        if(!adapter) continue;
        DXGI_ADAPTER_DESC1 desc{}; if(FAILED(adapter->GetDesc1(&desc)) || desc.VendorId!=0x10DE || (desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
        auto gpu=ParseGpuSeries(desc.Description);
        if(IsSupportedGpu(gpu) && std::find(result.begin(),result.end(),gpu)==result.end()) result.push_back(gpu);
    }
    return result;
}
GpuSeries DefaultGpuSeries() { const auto found=DetectedGpuSeries(); return found.size()==1 ? found.front() : GpuSeries::Unknown; }
bool MatchesHardware(GpuSeries gpu) { const auto found=DetectedGpuSeries(); return IsSupportedGpu(gpu) && std::find(found.begin(),found.end(),gpu)!=found.end(); }
void RequireHardware(GpuSeries gpu) { if(!MatchesHardware(gpu)) throw std::runtime_error(TrUtf8("Seçilen seri bu bilgisayarda algılanmadı. Paket indirilebilir; oyuna kurmak için bilgisayarınızdaki ekran kartı serisini seçin.")); }
std::wstring NeuralPackageLabel(GpuSeries gpu) {
    if(IsOlderGpu(gpu)) return GpuLabel(gpu)+Tr(L" — 310.8.SF-v2 (topluluk / FP16). FPS belirgin düşebilir; oyun içi uyumluluk değişir.");
    if(gpu==GpuSeries::RTX40) return Tr(L"RTX 40 — 310.8.0-RTX40 topluluk paketi.");
    if(gpu==GpuSeries::RTX50) return Tr(L"RTX 50 — 310.8.0 paketi.");
    return Tr(L"Paket indirmek için ekran kartı serisini seçin. Desteklenmeyen kartlara kurulum yapılmaz.");
}
}
