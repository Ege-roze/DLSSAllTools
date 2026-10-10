# DLSS 5 integration — v0.5

This is an independent C++ installer. No third-party DLL is embedded in the application.
Packages are downloaded only on request, verified by fixed SHA-256 checksums, extracted
as data with Windows tar, and copied using a durable per-game journal. Installers and
downloaded scripts are never executed. `src/neural_catalog.hpp` is the complete release,
file and digest inventory (13 components, 35 entries across four GPU series (three NR variants)).

## Upstream reference

- DLSS5-Swapper, Rakan Alkhaldi: https://github.com/rakanki911/DLSS5-Swapper/tree/9fb0b7c3563d350aa46eaa01d3e5ca4cbab4a7cb
- MIT notice retained at `third_party/LICENSE-dlss5-swapper.txt`. The native/Feeder layouts
  and Feeder configuration values were adapted from its installation/configuration code.
- Native uses one RenoDX consumer, ReShade Add-on and SR/NR runtimes. Feeder also uses the
  matching Feeder add-on, feed shader, VORT motion-vector shaders and ReShade headers.
- This implementation supports x64 DX11/DX12 only. It does not install OptiScaler,
  multipass, 32-bit host processes, Vulkan registry layers or legacy API translators.

## Fixed components

| Component | Version / upstream |
|---|---|
| ReShade Add-on x64 | 6.8.0, https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe |
| RenoDX consumer | 6.5.3, https://github.com/RankFTW/rhi-repo/releases/tag/renodx-dlss5-6.5.3 |
| DLSS SR | 310.9.1, https://github.com/RankFTW/rhi-repo/releases/tag/dlss-310.9.1 |
| DLSS NR, RTX20/30 community FP16 | 310.8.SF-v2, https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2 |
| DLSS NR, RTX40 community build | 310.8.0-RTX40, https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.0-RTX40 |
| DLSS NR, RTX50 | 310.8.0, https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.0 |
| Feeder | 1.17.0, https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/tag/v1.17.0 |
| VORT shaders | b410b9f0c0fbb83c8cb42164aaf1655fab386f4a, https://github.com/vortigern11/vort_Shaders |
| ReShade shader headers | ee30868391d4ad103db60489820102d8fd40e3c1, https://github.com/crosire/reshade-shaders |

RHI is a community distribution repository, not NVIDIA. The RTX40 binary is selected
explicitly by its RTX40 release tag; the RTX50-only Lecram runtime is not substituted for it.
Package integrity has been verified. Actual neural rendering on this laptop is left to
the user's in-game test, and is not inferred from copying DLLs successfully.

ReShade, RenoDX, Feeder and VORT license notices are retained in the cache and copied to
`DLSSAllTools-licenses` alongside the game executable. NVIDIA binaries remain subject
to NVIDIA's terms; the application's license does not relicense third-party components.

## Ownership and recovery

- MFG keeps its `.dlssalltools-install.json` receipt. DLSS5 uses a separate
  `.dlssalltools-dlss5/manifest.json` beside the EXE.
- A fixed destination allowlist prevents the journal from targeting MFG's `version.dll`
  or unrelated files. Paths cannot escape the EXE directory or use reparse points.
- All original bytes and incoming bytes are staged and verified before the first target
  changes. `preparing`, `installing`, `installed`, `restoring`, `restored` journal states
  permit recovery after interrupted copying or restoration.
- Only matching binary files may be removed/restored. External binary changes cause a
  preflight failure before restoration. Existing identical files are never adopted.
- An existing compatible ReShade Add-on DXGI proxy is reused. Other DXGI proxies require
  the explicit replacement option and are backed up. Known MFG DXGI payloads are blocked.
- Mutable INI/CFG files are saved again in `son-ayarlar` before originals are restored.
  Restored transaction folders remain archived beside the EXE. They can be large because
  they retain the original and staged packages; no broad cleanup removes user data.
- Only the SR runtime beside the selected EXE is updated; the game’s FG, RR and Streamline
  DLLs are retained. A newer SR runtime is not downgraded. A deeply nested engine SR DLL is
  not modified by this first integration.
- Feeder selects a separate local preset, keeping the user's old preset file intact;
  removal restores the previous ReShade configuration. Use the native route for Cyberpunk.

## Verification

83 isolated tests cover existing MFG/driver behavior and journal recovery. Seven existing plus twelve GPU/package
integration tests deploy the actual pinned packages into fake game directories, without
loading the DLLs. Both Release and Debug configurations are tested. The manager's own
downloader verifies the full RTX20/30, RTX40 and RTX50 packages. GUI checks cover the separate DLSS5
dialog, automatic RTX40 selection and native route selection for Cyberpunk.

RTX20/30 MFG uses a separately scoped journal in `.dlssalltools-mfg-sm86`; its allowlist does not overlap the Neural file set. See [GPU-PACKAGES.md](GPU-PACKAGES.md) for source pins, evidence and limits.
