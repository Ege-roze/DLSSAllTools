#pragma once
#include "core.hpp"
#include <array>
#include <memory>
#include <optional>

namespace dlss {
inline constexpr uint32_t FgEnableId = 0x10E41E03, FgPresetId = 0x10E41DF1;
inline constexpr std::array<uint32_t, 2> FgIds{FgEnableId, FgPresetId};
inline constexpr std::array<uint32_t, 2> FgValues{1, 2};
struct DriverValue {
    bool present = false, local = false;
    uint32_t value = 0;
    bool operator==(const DriverValue&) const = default;
};
struct DriverSnapshot {
    std::wstring profile;
    std::vector<std::wstring> applications;
    std::array<DriverValue, 2> settings;
    bool baseProfile = false;
};
// Writes are staged in a driver session; Save is the only persistent driver operation.
class ProfileStore {
public:
    virtual ~ProfileStore() = default;
    virtual DriverSnapshot Read() = 0;
    virtual void Stage(size_t index, std::optional<uint32_t> value) = 0;
    virtual void Save() = 0;
    virtual void Reload() = 0;
};
std::unique_ptr<ProfileStore> OpenDriverProfile(const Game& game);
struct PresetFixState {
    bool managed = false, enabled = false, conflict = false, alreadySet = false;
    std::wstring message, profile;
};
fs::path PresetBackupPath(const fs::path& state, const std::wstring& profile);
PresetFixState InspectPresetFix(const Game& game, const fs::path& state, ProfileStore& driver);
void SetPresetFix(const Game& game, const fs::path& state, ProfileStore& driver, bool enable);
json DriverReport(const Game& game);
}
