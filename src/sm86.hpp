#pragma once
#include "neural.hpp"
namespace dlss {
inline constexpr wchar_t Sm86Journal[] = L".dlssalltools-mfg-sm86";
struct Sm86File {const wchar_t* name; const wchar_t* upstream; const char* hash;};
inline constexpr Sm86File Sm86Files[]{
    {L"version.dll",L"version.dll","c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838"},
    {L"winmm.dll",L"alternatives/winmm.dll","bc3cef25c1ccbbdccdd93db9fae845104bb6c71fd433240e26641881a7e46658"},
    {L"dbghelp.dll",L"alternatives/dbghelp.dll","dfe7f44baa68d237dfe040d4b4cfc9bd44b01e661f3051996db1144e6bb388cb"},
    {L"dinput8.dll",L"alternatives/dinput8.dll","4fbad08bb9360a1cd1309a82586353484fe64b7220199344ab83fae308176c6b"},
    {L"dlssg_sm86.ini",L"dlssg_sm86.ini","2616857ee29ec61e33c8b52e1b50f4c93cb5339adbb13b73f0ae71a722427a43"}
};
class Sm86Manager {
public:
    explicit Sm86Manager(fs::path state):state_(std::move(state)){}
    fs::path Cache() const;
    bool PackageReady() const;
    void Prepare(const Progress& progress) const;
    Inspection Inspect(const Game& game) const;
    void Install(const Game& game,GpuSeries gpu,const Progress& progress) const;
    void Uninstall(const Game& game,const Progress& progress) const;
private:
    fs::path state_;
};
}
