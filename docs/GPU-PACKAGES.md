# RTX serisine göre paketler — v0.5 / 2026-10-09

| Seri | DLSS 5 / Neural Rendering | MFG |
|---|---|---|
| RTX 20 | ShortFuse 310.8.SF-v2, FP16 topluluk paketi | DLSSG for SM86 0.3.5, 310.9, varsayılan 4x |
| RTX 30 | ShortFuse 310.8.SF-v2, FP16 topluluk paketi | DLSSG for SM86 0.3.5, 310.9, varsayılan 4x |
| RTX 40 | 310.8.0-RTX40, mevcut paket | Mevcut 1.3.3 Hotfix 2 / 1.4.1 Hotfix 1 |
| RTX 50 | 310.8.0, mevcut paket | Oyunun yerel MFG desteği; kilit açma paketi indirilmez |

## Kullanım

Ana ekranın **Ekran kartı** listesinden seriyi seçin. Tek bir destekli seri algılanırsa otomatik seçilir; birden fazla seri veya tanınmayan kartta seçim istenir. Laptop ve SUPER/Ti modelleri tanınır. GTX, AMD/Intel ve Quadro/RTX A/RTX PRO kartları GeForce serileriyle karıştırılmaz. RTX 2050 Laptop serisi 20 olarak gösterilir; bu modelde özel oyun/donanım testi yapılmamıştır.

- **MFG paketini indir** seçili seriye uygun sürümü hazırlar. 20/30 aynı paketi paylaşır. 40 serisinin sürüm listesi bu serilerde kapanır; elle yanlış RTX40 DLL içe aktarma da kapalıdır.
- **DLSS 5 kur / kaldır…** ana ekrandaki seri seçimini alır. Pencerede değiştirilebilir; seçili runtime ve FP16 performans bilgisi gösterilir.
- Dosyalar önbellekte varsa SHA-256 kontrolünden sonra tekrar kullanılır. Her seri yalnızca kendi NR runtime'ını ve ortak bileşenleri indirir.
- Başka bir bilgisayar için farklı seri paketi indirilebilir. Fakat seçili seri bu bilgisayarın NVIDIA donanım listesinde yoksa **oyuna kurulum kapalıdır**. Kaldırma donanımdan/seçimden bağımsız olarak mevcut kurulum kaydını izler.
- RTX20/30 MFG için Windows x64, oyunun **DirectX 12** sürümü ve mevcut DLSS Frame Generation entegrasyonu gerekir. EXE'nin yüklediği boş `version.dll`, `winmm.dll`, `dbghelp.dll` veya `dinput8.dll` yolu aranır. Bulunamazsa kurulum yapılmaz. Bilinen Witcher 3 DX12 yolu desteklenir. Vulkan/DX11'e otomatik MFG kurulmaz.
- MFG 0.3.5 oyun ayarındaki 2x/3x/4x seçimini kullanır; RTXMFG'nin Backspace menüsü bu pakette yoktur. Otomatik 6x zorlanmaz.
- DLSS5 mevcut x64 DX11/12 native/Feeder kapsamını korur. Her oyunda çalışacağı veya FPS artışı sağlayacağı anlamına gelmez; 20/30 FP16 yolu belirgin FPS kaybına neden olabilir.

## Kaynaklar ve kanıt düzeyi

1. **MFG:** [DLSSG for SM86 0.3.5](https://github.com/sdli1995/dlssg_for_sm86/releases/tag/0.3.5). Sabit kaynak/dosya commit'i: `9621db573e07ed54f50c15bbb585ed9a7bdfac28`. [İngilizce README](https://github.com/sdli1995/dlssg_for_sm86/blob/9621db573e07ed54f50c15bbb585ed9a7bdfac28/README.en.md) RTX 2080 Ti desteğini, 3070/3080 Ti geliştirici ölçümlerini ve Cyberpunk/Wukong/FH6 4x geri bildirimlerini anlatır. 0.3.5, FG yeniden oluşturulduğunda oluşabilen bozuk kare/çökme sorununu düzeltir; 3070 üzerinde 15 yeniden oluşturma testi raporlanır. Bu, her kart/oyun kombinasyonunun test edildiği anlamına gelmez.
2. **DLSS5:** [310.8.SF-v2 dağıtımı](https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2); [DLSS5-Autopilot GPU tablosu](https://github.com/Kizzuwatnaa/DLSS5-Autopilot#your-graphics-card) RTX20/30 için SF/SF-v2 FP16 yolunu belgeliyor. [GTA IV v1.2.1 yayını](https://github.com/JungleHam/GTAIV-DLAA-DLSS4.5-DLSS5-FSR/releases/tag/v1.2.1) aynı sabit SF-v2 paketi ve DLL hash'ini kullanıyor. Bunlar paket ve hedef mimari desteğinin kanıtıdır; bu uygulamanın RenoDX 6.5.3 + Feeder 1.17.0 kombinasyonunun her 20/30 kartta oyun içinde denenmiş olduğu iddia edilmez.
3. **Yerel doğrulama:** Bilgisayarda RTX 4060 Laptop var. 20/30 kartta oyun içi test yapılmadı. Release/Debug derlemeleri, dört seri için paket doğrulaması, dosya kur/kaldır, çakışma ve yarım işlem testleri yapılır. Mod DLL'leri test sürecine yüklenmez. Bu ayrım özellikle korunur: başarılı dosya kopyalama, çalışan neural render veya ölçülmüş FPS değildir.

## Sabit dosyalar

DLSS5 SF-v2 arşivi: `1da35941894994eb087e017577829e492454e9bae3a6a9397027069ceb74955c`.
Çıkan `nvngx_dlssnr.dll`: `6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927`.

MFG dosyaları değişmeyen commit URL'sinden ayrı indirilir; her forwarding DLL kendi adıyla kullanılır, `version.dll` başka adla yeniden adlandırılmaz:

| Dosya | SHA-256 |
|---|---|
| version.dll | c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838 |
| alternatives/winmm.dll | bc3cef25c1ccbbdccdd93db9fae845104bb6c71fd433240e26641881a7e46658 |
| alternatives/dbghelp.dll | dfe7f44baa68d237dfe040d4b4cfc9bd44b01e661f3051996db1144e6bb388cb |
| alternatives/dinput8.dll | 4fbad08bb9360a1cd1309a82586353484fe64b7220199344ab83fae308176c6b |
| dlssg_sm86.ini | 2616857ee29ec61e33c8b52e1b50f4c93cb5339adbb13b73f0ae71a722427a43 |

## Geri alma ve dağıtım

RTX20/30 MFG için ayrı `.dlssalltools-mfg-sm86/manifest.json` kaydı kullanılır. Bütün dosyalar doğrulanıp kayıt kalıcı yazılmadan oyuna kopyalama başlamaz. DLSS5'in DXGI/NR dosyaları bu kaydın izin listesine dahil değildir; tersine DLSS5 kaydı MFG utility dosyalarına erişemez.

MFG kaldırılırken sonradan düzenlenmiş `dlssg_sm86.ini` arşivlenir, özgün oyun dosyaları korunur. Arşiv `.dlssalltools-mfg-sm86-restored-...` olarak oyun yanında kalır. Modun oyun sırasında oluşturduğu loglar ve `%LOCALAPPDATA%\DlssgSm86` runtime önbelleği bu yöneticinin kurulum dosyaları değildir; otomatik silinmez. RTX40'ın önceki tek DLL kurulum kaydı uyumluluğu korunur.

Portable uygulamada üçüncü taraf DLL gömülü değildir. İndirme işlevi entegredir; internet erişimi ilk hazırlamada gerekir. MFG üçüncü taraf bildirimi `third_party/NOTICE-dlssg-sm86.txt` içindedir. Upstream proje kendi kaynak lisansını GPLv3 olarak belirtir; NVIDIA runtime/kernellerini bu lisansla yeniden lisanslamaz. Bu yönetici upstream kodunu derlemeye katmaz, yalnızca kullanıcının isteğiyle sabit dosyaları indirir.
