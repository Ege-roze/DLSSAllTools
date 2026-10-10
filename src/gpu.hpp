#pragma once
#include "core.hpp"
namespace dlss {
enum class GpuSeries { Unknown, RTX20=20, RTX30=30, RTX40=40, RTX50=50 };
inline constexpr GpuSeries GpuChoices[]{GpuSeries::Unknown,GpuSeries::RTX20,GpuSeries::RTX30,GpuSeries::RTX40,GpuSeries::RTX50};
GpuSeries ParseGpuSeries(const std::wstring& name);
std::vector<GpuSeries> DetectedGpuSeries();
GpuSeries DefaultGpuSeries();
std::wstring GpuLabel(GpuSeries gpu);
std::string GpuCode(GpuSeries gpu);
bool IsSupportedGpu(GpuSeries gpu);
bool IsOlderGpu(GpuSeries gpu);
bool MatchesHardware(GpuSeries gpu);
void RequireHardware(GpuSeries gpu);
std::wstring NeuralPackageLabel(GpuSeries gpu);
}
