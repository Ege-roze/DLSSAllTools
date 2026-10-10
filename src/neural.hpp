#pragma once
#include "core.hpp"
#include "gpu.hpp"

namespace dlss {
void ShowNeuralDialog(HWND owner, const Game& game, const fs::path& state, GpuSeries selected);
using NeuralGpu = GpuSeries;
enum class NeuralRoute { Native, Feeder };
struct NeuralOptions {
    NeuralGpu gpu = NeuralGpu::RTX40;
    NeuralRoute route = NeuralRoute::Native;
    bool replaceProxy = false;
};
struct NeuralState {
    bool installed = false, recoverable = false, conflict = false, ready = false;
    std::wstring message;
};
class NeuralManager {
public:
    explicit NeuralManager(fs::path state) : state_(std::move(state)) {}
    fs::path Cache() const;
    bool PackageReady(NeuralGpu gpu) const;
    void Prepare(NeuralGpu gpu, const Progress& progress) const;
    NeuralState Inspect(const Game& game) const;
    void Install(const Game& game, const NeuralOptions& options, const Progress& progress) const;
    void Uninstall(const Game& game, const Progress& progress) const;
private:
    fs::path state_;
};
// Narrow transaction layer, also exercised with isolated file fixtures.
struct NeuralFile { fs::path source; std::wstring destination; std::string hash; bool settings = false; };
enum class FileSet { Neural, Sm86 };
void InstallNeuralFiles(const Game& game, const std::vector<NeuralFile>& files, const json& metadata,
                        const Progress& progress, FileSet kind=FileSet::Neural);
void RestoreNeuralFiles(const Game& game, const Progress& progress, FileSet kind=FileSet::Neural);
NeuralState InspectManagedFiles(const Game& game, FileSet kind);
std::string NeuralIniSet(const std::string& text, const std::string& section, const std::string& key, const std::string& value);
}
