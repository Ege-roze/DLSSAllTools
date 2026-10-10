// Fixed, SHA-256 checked component catalogue. See docs/DLSS5-SOURCES.md.
#pragma once
inline constexpr char NeuralCatalogJson[] = R"catalog([
  {
    "id": "nr2030",
    "url": "https://github.com/RankFTW/rhi-repo/releases/download/dlssnr-310.8.SF-v2/nvngx_dlssnr_310.8.SF-v2.zip",
    "sha256": "1da35941894994eb087e017577829e492454e9bae3a6a9397027069ceb74955c",
    "archive": "nr2030.zip",
    "gpu": "20,30",
    "feeder": false,
    "files": [
      {
        "entry": "nvngx_dlssnr.dll",
        "path": "nvngx_dlssnr.dll",
        "sha256": "6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927"
      }
    ]
  },
  {
    "id": "nr40",
    "url": "https://github.com/RankFTW/rhi-repo/releases/download/dlssnr-310.8.0-RTX40/nvngx_dlssnr_310.8.0-RTX40.zip",
    "sha256": "46124cfaef532ad5f6da07494772ea8c1b3e719f934e254385697f38d1289e3f",
    "archive": "nr40.zip",
    "files": [
      {
        "entry": "nvngx_dlssnr.dll",
        "path": "nvngx_dlssnr.dll",
        "sha256": "4b8d19bc3eff58a084f5eca7489c921501c203450169fb82ff4f649a4482ba05"
      }
    ],
    "gpu": "40",
    "feeder": false
  },
  {
    "id": "nr50",
    "url": "https://github.com/RankFTW/rhi-repo/releases/download/dlssnr-310.8.0/nvngx_dlssnr_310.8.0.zip",
    "sha256": "388c0a7912e15ec911b9c9e11a692142b11fe387ddf2b637d8c358138fffb3ac",
    "archive": "nr50.zip",
    "files": [
      {
        "entry": "nvngx_dlssnr.dll",
        "path": "nvngx_dlssnr.dll",
        "sha256": "e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e"
      }
    ],
    "gpu": "50",
    "feeder": false
  },
  {
    "id": "sr",
    "url": "https://github.com/RankFTW/rhi-repo/releases/download/dlss-310.9.1/nvngx_dlss_310.9.1.zip",
    "sha256": "aaba83b288bd145c3808e8d7a0ba03cc8c8676d18ad984b1bfa6563046a3ba37",
    "archive": "sr.zip",
    "files": [
      {
        "entry": "nvngx_dlss.dll",
        "path": "nvngx_dlss.dll",
        "sha256": "3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983"
      }
    ],
    "gpu": "all",
    "feeder": false
  },
  {
    "id": "renodx",
    "url": "https://github.com/RankFTW/rhi-repo/releases/download/renodx-dlss5-6.5.3/renodx-dlss5_6.5.3.zip",
    "sha256": "553b1619b9e5ddfbcb4ebc7f2f3bffff9256a48a25b988f4817f5c63f4caa1de",
    "archive": "renodx.zip",
    "files": [
      {
        "entry": "renodx-dlss5.addon64",
        "path": "renodx-dlss5.addon64",
        "sha256": "342341f669f1d64e0c70c8593a07a2fab5075e073dfae97c331c9a6776260a0a"
      }
    ],
    "gpu": "all",
    "feeder": false
  },
  {
    "id": "reshade",
    "url": "https://reshade.me/downloads/ReShade_Setup_6.8.0_Addon.exe",
    "sha256": "afe4c8f13048306307983b8b3d41d5bf00a86820440b0e57dea10950e1176445",
    "archive": "reshade.exe",
    "files": [
      {
        "entry": "ReShade64.dll",
        "path": "dxgi.dll",
        "sha256": "0cee63f9c9f13f3ac909c5b4903f4dbb4b719a7ab3b4f13b0deaf83c814b94f7"
      }
    ],
    "gpu": "all",
    "feeder": false
  },
  {
    "id": "feeder",
    "url": "https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/download/v1.17.0/DLSS5-Feeder-1.17.0.zip",
    "sha256": "11a96b36ae89ef75b3cff0e03849db35591143fe4df171986502905809469e12",
    "archive": "feeder.zip",
    "files": [
      {
        "entry": "dlss5-feed.addon64",
        "path": "dlss5-feed.addon64",
        "sha256": "6854d012eac307021cd31c978bafd42f1e22c5b7b2922c80e0a0010d428a3fd7"
      },
      {
        "entry": "reshade-shaders/Shaders/DLSS5_Feed.fx",
        "path": "DLSSAllTools-shaders/Shaders/DLSS5_Feed.fx",
        "sha256": "c4ba1610df8e1593faa7c203d6d0517058d3a8ecf2dfda18ca67849844391456"
      }
    ],
    "gpu": "all",
    "feeder": true
  },
  {
    "id": "vort",
    "url": "https://codeload.github.com/vortigern11/vort_Shaders/zip/b410b9f0c0fbb83c8cb42164aaf1655fab386f4a",
    "sha256": "231ba34a75556f9943e359559a89b0d0cc2caa322d9dcdee5630061bf9fe13b6",
    "archive": "vort.zip",
    "files": [
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/vort_Motion.fx",
        "path": "DLSSAllTools-shaders/Shaders/vort_Motion.fx",
        "sha256": "71ef3086ccf86792f5a333346eebb7b857434583712d3a51b9998db99e562edc"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/LICENSE",
        "path": "DLSSAllTools-licenses/VORT.txt",
        "sha256": "800bacb0f9db4df8203f0dad1f99947109a44f4097380807e0483bd62dee0d41"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_ACES.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_ACES.fxh",
        "sha256": "967cb4914457f18cc91e8844e6f7ccef0b353e4091e38f9c8667d36d45309b61"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Bloom.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Bloom.fxh",
        "sha256": "a3ab7942b73c61dcabe9b83ae42dafe65956c7a60d627aea27ede48a7cfb3c04"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_BlueNoise.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_BlueNoise.fxh",
        "sha256": "a1eca01ddc6bdfef2b0c52601d297c59d7dabd71039d1489b7092532407cb455"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_ColorChanges.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_ColorChanges.fxh",
        "sha256": "0cbd91dff9aa5ab615a206f56ba760378810a5540d4f2928e35138c7980b1538"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_ColorTex.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_ColorTex.fxh",
        "sha256": "f5eef2db313c73b0419503295731c03bc4365fe1ed219f4afa618707b59e8d6c"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Defs.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Defs.fxh",
        "sha256": "a00e99ece8b07899a7918a5248a2d603fd7f98882c795e3d31b9470cfe70a61c"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Depth.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Depth.fxh",
        "sha256": "92d4f0a93b3945452c861e580117bb875bb594157694b3c8741790eaf3826205"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Filters.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Filters.fxh",
        "sha256": "8d98a78f2fd7fe91f34bf4044d81094bd40fbb2c7b8f520ef66c5043f720f98d"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_HDRTex.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_HDRTex.fxh",
        "sha256": "5d3d002a6d4d0fc6e67dcdd4a68497163268fbb590c7477220dac8f1293f916e"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Motion_UI.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Motion_UI.fxh",
        "sha256": "29783705b01d7b83a408d29a9d4b72c5a90012a91fb05ebdf1166ba9e9aeac2b"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_MotionBlur.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_MotionBlur.fxh",
        "sha256": "67edbc5db1645d3c10c08375eb0bec0911c59c4fc1d3dde2d3e9c46a292ea35b"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_MotionBlur_Cheap.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_MotionBlur_Cheap.fxh",
        "sha256": "0837c8151a50ef57c77bab7a3b14dd4c3b92354e4021ee37f54eb47f3f5589c7"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_MotionUtils.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_MotionUtils.fxh",
        "sha256": "2d353b23cea6961ca40308a6aebf1e56d509485a728399d5f6b36427b7023027"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_MotionVectors.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_MotionVectors.fxh",
        "sha256": "22d5c8f8a5f2cccee0ddde994ba04a7bc5379c54b1b3e9c7cf7d46f1f8ead87d"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_OKColors.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_OKColors.fxh",
        "sha256": "5a0a8468e97eaa9e8e018246e12002a86e3fb0c7a9cb97a3bc38390fc3f854e2"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Static_UI.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Static_UI.fxh",
        "sha256": "ccd75ef28359b5324cf479fe568fbad7586ac203f6b1921f1b6fae48ae19bef2"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_TAA.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_TAA.fxh",
        "sha256": "9b5c701fdacb37f15e565ca1aa3a861e4df3f55db493cd6589d509927ed08ede"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Shaders/Includes/vort_Tonemap.fxh",
        "path": "DLSSAllTools-shaders/Shaders/Includes/vort_Tonemap.fxh",
        "sha256": "9413eef6190303e920356310bc3aa2a270c052a1bcfde44f024f0a8986095f30"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Textures/vort_BlueNoise.png",
        "path": "DLSSAllTools-shaders/Textures/vort_BlueNoise.png",
        "sha256": "050d26db9283fbaece3b6883e300d22403631d83a12a5f2f79dc8b789b860305"
      },
      {
        "entry": "vort_Shaders-b410b9f0c0fbb83c8cb42164aaf1655fab386f4a/Textures/vort_MLUT.png",
        "path": "DLSSAllTools-shaders/Textures/vort_MLUT.png",
        "sha256": "acb25ed926e7e847aeb0ef521745287d4ddda3c7f22e472ee42d61bb3064bbd7"
      }
    ],
    "gpu": "all",
    "feeder": true
  },
  {
    "id": "ReShade.fxh",
    "url": "https://raw.githubusercontent.com/crosire/reshade-shaders/ee30868391d4ad103db60489820102d8fd40e3c1/Shaders/ReShade.fxh",
    "sha256": "6dabfbbaf968c3871905d2ea17f96572ff7b1cec01310b5d0e5252b66b30174f",
    "archive": "ReShade.fxh",
    "files": [
      {
        "entry": "",
        "path": "DLSSAllTools-shaders/Shaders/ReShade.fxh",
        "sha256": "6dabfbbaf968c3871905d2ea17f96572ff7b1cec01310b5d0e5252b66b30174f"
      }
    ],
    "gpu": "all",
    "feeder": true
  },
  {
    "id": "ReShadeUI.fxh",
    "url": "https://raw.githubusercontent.com/crosire/reshade-shaders/ee30868391d4ad103db60489820102d8fd40e3c1/Shaders/ReShadeUI.fxh",
    "sha256": "78adf672df47460297eb9fe6dd238d2aafa24510b52b84feb1a745dff70eb901",
    "archive": "ReShadeUI.fxh",
    "files": [
      {
        "entry": "",
        "path": "DLSSAllTools-shaders/Shaders/ReShadeUI.fxh",
        "sha256": "78adf672df47460297eb9fe6dd238d2aafa24510b52b84feb1a745dff70eb901"
      }
    ],
    "gpu": "all",
    "feeder": true
  },
  {
    "id": "ReShade-LICENSE.md",
    "url": "https://raw.githubusercontent.com/crosire/reshade/v6.8.0/LICENSE.md",
    "sha256": "237ded5b8344f820113efab1e65e91e1f159d9202c5b4856606a0590d3ffdab0",
    "archive": "ReShade-LICENSE.md",
    "files": [
      {
        "entry": "",
        "path": "DLSSAllTools-licenses/ReShade-LICENSE.md.txt",
        "sha256": "237ded5b8344f820113efab1e65e91e1f159d9202c5b4856606a0590d3ffdab0"
      }
    ],
    "gpu": "all",
    "feeder": false
  },
  {
    "id": "RenoDX-LICENSE",
    "url": "https://raw.githubusercontent.com/clshortfuse/renodx/main/LICENSE",
    "sha256": "e9e2da3991a48d9c09b710c9ddbf5673275b6d569c976bcffab3111d0c509d51",
    "archive": "RenoDX-LICENSE",
    "files": [
      {
        "entry": "",
        "path": "DLSSAllTools-licenses/RenoDX-LICENSE.txt",
        "sha256": "e9e2da3991a48d9c09b710c9ddbf5673275b6d569c976bcffab3111d0c509d51"
      }
    ],
    "gpu": "all",
    "feeder": false
  },
  {
    "id": "Feeder-LICENSE",
    "url": "https://raw.githubusercontent.com/jlrouzies-fr/DLSS5-Feeder/v0.7.0/LICENSE",
    "sha256": "6562d5a5e3d7534711e34f4b34335f23f067acc839ae5274c1250bf5f4654b8b",
    "archive": "Feeder-LICENSE",
    "files": [
      {
        "entry": "",
        "path": "DLSSAllTools-licenses/Feeder-LICENSE.txt",
        "sha256": "6562d5a5e3d7534711e34f4b34335f23f067acc839ae5274c1250bf5f4654b8b"
      }
    ],
    "gpu": "all",
    "feeder": false
  }
])catalog";
