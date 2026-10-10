# DLSS All Tools — 0.6

Windows 10/11 x64 için Türkçe, İngilizce ve Basitleştirilmiş Çince MFG ve DLSS 5 kurulum yöneticisi. C++20 / Win32. MFG ve DLSS 5 ayrı ayrı kurulup kaldırılır.

## Dil / Language / 语言

Sağ üstteki dil listesinden **Türkçe**, **English** veya **简体中文** seçin. Değişiklik anında uygulanır ve uygulama yeniden açıldığında korunur. İşlem sürerken dil seçimi geçici olarak kapalıdır. Ana pencere, uygulama mesajları ve DLSS 5 / Preset B pencereleri aynı dili kullanır. Windows'un dosya seçicisi ve sistem düğmeleri Windows dilini izler; indirilen modların oyun içi menüleri kendi dil ayarlarını kullanır.

Choose **English** from the top-right language menu. Your choice is saved automatically and applies to the app's main window, messages, DLSS 5 and Preset B dialogs.

在右上角的语言菜单中选择 **简体中文**。所选语言会自动保存，并应用于主窗口、应用消息、DLSS 5 和 Preset B 对话框。

Dil tercihi `%LOCALAPPDATA%\DLSSAllTools\ui-preferences.json` içinde tutulur. Mod sürümü tercihi ve oyun kayıtlarından ayrıdır. Çeviriler EXE içinde bulunur; ek dil paketi indirilmez. Geliştiriciler için: [LOCALIZATION.md](docs/LOCALIZATION.md).

## Ekran kartına göre paket seçimi

**Ekran kartı** listesinden RTX 20 / 30 / 40 / 50 serisini seçin. Algılanan tek seri otomatik seçilir. Paket seriye göre sabitlenir; seçilen seri bilgisayarda yoksa indirme yapılabilir fakat oyuna kurulum engellenir. Tanınmayan kart otomatik RTX40 sayılmaz.

| Seri | DLSS 5 | MFG |
|---|---|---|
| RTX 20 / 30 | 310.8.SF-v2, FP16 topluluk paketi | DLSSG for SM86 0.3.5, 2x/3x/4x |
| RTX 40 | 310.8.0-RTX40 | 1.3.3 Hotfix 2 veya 1.4.1 Hotfix 1 |
| RTX 50 | 310.8.0 | Oyunun yerel MFG desteği; mod gerekmez |

RTX20/30 DLSS5 performansı ağır olabilir. Paket desteği, her oyunda test edilmiş destek anlamına gelmez. MFG geliştiricisi 2080 Ti / 3070 / 3080 Ti testlerini belgeliyor; bu bilgisayarda 20/30 donanım testi yapılmadı. Kaynaklar, sürüm seçiminin gerekçesi ve tam kapsam: [GPU-PACKAGES.md](docs/GPU-PACKAGES.md).

## DLSS 5 kullanımı
1. Oyunu kapatın. Listeden oyunu seçip **DLSS 5 kur / kaldır…** düğmesine basın.
2. Ana ekrandaki seri seçimi bu pencereye gelir; gerekirse **RTX 20/30/40/50** seçimini değiştirin. İndirilecek runtime ve performans bilgisi altında gösterilir.
3. Cyberpunk gibi DLSS içeren oyunlarda **Oyunun mevcut DLSS özelliğini kullan** seçili kalsın. DLSS bulunmayan oyunlarda **DLSS olmayan oyun — Feeder** seçilebilir.
4. **DLSS 5'i kur** eksik bileşenleri indirir, SHA-256 ile doğrular, değişecek dosyaları yedekler ve kurar. **Paketi indir** yalnızca hazırlık yapar. Önceden doğrulanmış önbellek varsa yeniden indirilmez.
5. Oyunu açın. **Home → Add-ons → RenoDX DLSS5** bölümünden ayarlayın. Mevcut DLSS yönteminde oyunun grafik ayarlarından DLSS'yi de etkinleştirin. Bu kurulum dosya yönetimini otomatikleştirir; oyun içi ayarlar oyunda yapılır.
6. Kaldırmak için oyunu kapatıp aynı pencereden **Kaldır / Geri al** seçin. MFG ayrı kalır.

**Mevcut ReShade:** Add-on desteği doğrulanırsa aynı `dxgi.dll` kullanılır. Başka veya tanınamayan bir grafik yükleyicisi varsa otomatik üzerine yazılmaz. Onu değiştirmek istiyorsanız **Mevcut grafik yükleyicisini yedekleyip değiştir** kutusunu seçin; önceki dosya kaldırmada geri gelir. `dxgi.dll` içinde bilinen MFG paketi varsa bu seçenekle de değiştirilmez.

DLSS 5 entegrasyonu **64 bit DirectX 11/12** ve **RTX 20/30/40/50 paketlerini** kapsar. ReShade 6.8 Add-on, RenoDX 6.5.3, DLSS SR 310.9.1, karta uygun DLSS NR 310.8; Feeder yolunda ayrıca Feeder 1.17.0 ve hareket vektörü shader dosyaları kullanılır. OptiScaler, Vulkan/OpenGL ve 32 bit oyunlar bu sürüme dahil değildir.

DLSS 5 kaydı EXE yanında `.dlssalltools-dlss5` klasöründe tutulur. Bunu kurulum devam ederken silmeyin. Kaldırmada orijinal dosyalar geri gelir; sonradan değişen ReShade ayarları da `.dlssalltools-dlss5-restored-.../son-ayarlar` içinde saklanır. Bu yedekler disk alanı kullanır. Diğer bir uygulama DLL'leri değiştirirse otomatik kaldırma durur ve dosyaları korur. Feeder kendi presetini seçer; önceki preset dosyası korunur ve kaldırmada tekrar seçilir.

Oyun içi görüntü ve performans kullanıcı tarafından sınanacaktır. Kurulum başarısı, DLSS 5'in görüntü üretmeye başladığını tek başına göstermez. Ayrıntılı bileşen kaynakları ve test kapsamı: [DLSS5-SOURCES.md](docs/DLSS5-SOURCES.md).

## MFG kullanımı
1. `DLSSAllTools.exe` dosyasını açın.
2. Steam ve Epic oyunları açılışta taranır. Listede yoksa **Oyun ekle** ile gerçek oyun EXE dosyasını seçin.
3. **Ekran kartı** serisini seçin. RTX20/30 için **SM86 0.3.5 · 4x** otomatik belirlenir. RTX40 için **MFG paketi** listesindeki 1.3.3 Hotfix 2 veya 1.4.1 Hotfix 1 seçilebilir; önceki sürüm tercihi korunur.
4. **MFG paketini indir** ile seriye uygun paketi hazırlayın. Daha önce hazırlanmışsa **MFG paketi hazır ✓** görünür. RTX40 için ayrıca seçili sürümün doğrulanmış RTXMFG.dll dosyası içe aktarılabilir.
5. Oyun kapalıyken listeden seçin. Uygun bir kurulum yolu bulunursa **Kur** etkinleşir.
6. Oyunda DLSS Frame Generation'ı açın. RTX20/30 paketinde çarpanı oyunun grafik ayarlarından seçin. RTX40 mod menüsü: **Backspace**.
7. Kaldırmak için oyunu tamamen kapatıp **MFG kaldır** seçin. RTX40 ayarları yerinde korunur; RTX20/30 INI ayarları kurulum arşivinde saklanır.

**RTX40 sürüm değiştirme:** Oyunu kapatın. Listeden istediğiniz mod sürümünü seçin, kurulu mod varsa **Kaldır**, ardından **Kur** kullanın. Yalnızca sürüm seçmek oyundaki dosyaları değiştirmez. İki sürüm ayrı önbelleklerde tutulur; eski ve yeni DLL'ler karıştırılmaz. Kaldırma, üstte hangi sürüm seçili olursa olsun oyunda kurulu sürümün doğrulanmış kaydını kullanır.

Cyberpunk 2077'de bildirilen 3x/4x titremesi için v1.3.3 Hotfix 2 karşılaştırma amacıyla eklenmiştir. Kullanıcı Preset B profil çözümü sonrasında Cyberpunk'ta sorunun düzeldiğini doğrulamıştır; yalnızca sürümü eskiye almanın etkisi ayrı doğrulanmamıştır. İlk paylaşılan kaynak ZIP'teki ASI dosyası yerine, aynı etiketin resmi ve bütün RTXMFG.dll yayını kullanılır.

## RTX40: 2x çalışıyor, 3x / 4x görüntüsü bozuluyorsa
1. Oyunu tamamen kapatın, uygulamada oyunu seçin.
2. **3x / 4x görüntü sorunu…** düğmesini açın.
3. **Preset B çözümünü etkinleştir** kutusunu işaretleyin. Ayarların doğrulandığı mesajını bekleyin; sonra oyunu yeniden açın.
4. Geri almak için aynı kutunun işaretini kaldırın. Önceki ayarlar geri yüklenir.

Bu işlem NVIDIA oyun profilinde **Enable DLSS-FG override = 1** ve **Override DLSS-FG preset = 2 (B)** ayarlarını birlikte kaydeder. Modun kendi JSON dosyasındaki Preset B seçimiyle aynı işlem değildir. İlgili kullanıcının bildirdiği çözüm: https://github.com/dashdogy/RTX40MFG-Unlock/issues/6#issuecomment-5558240358

Uygulama NVIDIA Profile Inspector kurmadan, NVIDIA'nın resmi NVAPI profil arayüzünü kullanır. Yalnızca seçilen oyunun profiline ait iki ayarı değiştirir. Genel profil, oyun DLL'leri, DLSS Super Resolution/Ray Reconstruction ve diğer grafik ayarları değiştirilmez. Bu sürücü seçeneği NVIDIA'nın DLSS-G override davranışını etkinleştirir; oyun içinde kesin sonuç garantisi değildir.

Her profil için önceki değerler `%LOCALAPPDATA%\DLSSAllTools\profile-fixes` içine kalıcı olarak yedeklenir. Özel değer varsa aynen, varsayılandan devralınıyorsa devralma durumu geri yüklenir. İşlem yarıda kalırsa yedek tutulur. Ayarlar sonradan başka bir araçla değiştirilmişse otomatik geri alma durur. Yedeği silmeyin. **Modu kaldırmak bu isteğe bağlı sürücü ayarını kaldırmaz**; aynı kutuyla ayrıca geri alın.

İki ayar zaten etkinse uygulama bunu gösterir ve mevcut ayarlara sahip çıkmaz. Desteklenmeyen sürücü, bulunamayan oyun profili, genel profil veya farklı programlarla paylaşılan profil için değişiklik yapılmaz. NVIDIA'nın Cyberpunk profiline bağlı `cplauncher.exe` aynı oyunun başlatıcısı olarak desteklenir. Yetki hatası alırsanız uygulamayı yönetici olarak açıp yeniden deneyin.

Arşivli paketler hem ZIP hem dosya SHA-256 değerleriyle doğrulanır. RTX20/30 MFG dosyaları değişmeyen kaynak commit URL'lerinden indirilir ve her biri sabit SHA-256 ile kontrol edilir. Uygulama kendisiyle birlikte üçüncü taraf mod DLL'si dağıtmaz; resmi yayından indirir. Bu sürüm otomatik olarak daha yeni mod sürümlerine geçmez.

## MFG destek sınırları
- Mod, oyunda mevcut Streamline DLSS Frame Generation entegrasyonuna ihtiyaç duyar. Bileşen dosyalarının bulunması gerçek çalışma garantisi değildir.
- Kurulabilir etiketi, uygun dosya ve yükleme yolu bulunduğunu gösterir. Oyun içindeki MFG başarısı bu sürüm tarafından ölçülmez.
- Steam/Epic otomatik tespiti ve diğer mağazalar için elle ekleme vardır. Birden fazla EXE arasından güvenilir seçim yapılamazsa kullanıcıdan EXE seçmesi istenir.
- Gerçek EXE'nin doğrudan içe aktardığı boş destekli DLL adı tercih edilir. The Witcher 3 DX12 için upstream'in belgelediği winmm.dll yolu da tanınır.
- Bink DLL değiştirme/zincirleme ve eski kurulumların otomatik devralınması bu sürüme dahil değildir. Mevcut dosyaların üzerine yazılmaz; özgün DLL yedeğini geri yükleme gerektiren bir işlem yapılmaz.
- Oyun klasörüne yazma izni gerekir. Korumalı bir konumda izin hatası alınırsa kullanıcı uygulamayı yönetici olarak açabilir; uygulama kendiliğinden yetki yükseltmez.
- Tarama bağlantılı klasörleri atlar; bir kurulum başına en fazla 15.000 girdi ve 8 klasör seviyesi inceler. Bu sınır nedeniyle bulunmayan oyunlar elle eklenebilir.
- Program oyunu veya mod DLL'sini kendi sürecine yüklemez. Ekran kartı serisi algılanır; farklı serinin bu bilgisayara kurulması engellenir. Oyun içindeki son çalışma durumunu runtime belirler.

## Veri ve geri alma
Kütüphane ve indirme önbelleği `%LOCALAPPDATA%\DLSSAllTools` içinde tutulur. Paketler `packages\<sürüm>\RTXMFG.dll`, son sürüm tercihi `preferences.json`, tarama uyarıları `last-scan.json` dosyasına kaydedilir. Uygulama yalnızca kendi önbelleğindeki doğrulanmış paketi otomatik algılar; İndirilenler/Masaüstü klasörlerini paket aramak için taramaz.

RTX40 için oyun EXE'sinin yanında `.dlssalltools-install.json` kurulum kaydı bulunur. RTX20/30 MFG için `.dlssalltools-mfg-sm86/manifest.json` kullanılır; kaldırılmış işlemin yedeği `.dlssalltools-mfg-sm86-restored-...` adıyla tutulur. Kayıt DLL kopyalanmadan önce kalıcı yazılır. Kaldırma, kayıttaki hedefin bilinen resmi DLL hash'iyle eşleşmesini gerektirir. Dosya değişmişse veya başka programa aitse silinmez. Yarım kalan kurulumlar **Kaldır** ile temizlenebilir; yarım/bozuk bir DLL söz konusuysa otomatik silme durur ve elle inceleme gerekir.

MFG kurulumu mevcut oyun dosyalarının üzerine yazmaz. DLSS 5 kurulumu değiştireceği dosyaları ayrıca yedekler. `RTXMFG-Universal.json` gibi kullanıcı ayarları kaldırmada korunur. Kurulum kayıtları olmadan eski kurulumlar otomatik sahiplenilmez.

## Geliştirme
`dlssalltools.slnx` dosyasını Visual Studio 2026'da açın. **Release / x64** veya **Debug / x64** seçin ve derleyin. C++ masaüstü iş yükü, v145 ve Windows SDK gereklidir. Çıktı: `out\x64\Release\DLSSAllTools.exe`.

Kullanılan JSON kütüphanesi projeye dahil edilmiştir; normal derleme ağ erişimi istemez. Statik C++ çalışma zamanı kullanılır. PDB sunucusu bağımlılığını önlemek için derleyici hata ayıklama bilgisi nesne dosyasına yazılır (`/Z7`).

İzole test komutu (oyun dosyaları değiştirilmez):

```powershell
& '.\out\x64\Release\DLSSAllTools.exe' --self-test 'C:\TestSonuclari' 'C:\Paket-1.4.1\RTXMFG.dll' 'C:\Paket-1.3.3\RTXMFG.dll'
```

İki resmi DLL verildiğinde 83 test çalışır. Ayrıca `--gpu-package-tests <çıktı-klasörü> <önbellek-klasörü>` 12 seri/paket entegrasyon testini, `--neural-package-tests <çıktı-klasörü> <önbellek-klasörü>` 7 DLSS5 regresyon testini çalıştırır. Test yalnızca yeni oluşturduğu fixture klasörünü değiştirir; modun gerçek oyun performansını sınamaz. Test raporu `test-results.txt` içinde oluşur. GUI uygulaması olduğundan komut satırından çalıştırırken süreç bitişini bekleyen bir yöntem kullanın.

## Devam kayıtları
`docs\PROGRESS.md` tamamlanan işleri, doğrulamaları ve sonraki geliştirme alanlarını içerir. Masaüstündeki `.checkpoints` klasöründe başlangıç proje yedeği ve geliştirme kontrol noktaları bulunur.

## Kaynaklar
- RTX20/30 MFG: https://github.com/sdli1995/dlssg_for_sm86/releases/tag/0.3.5
- RTX20/30 DLSS5: https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2
- Mod ve kurulum belgesi: https://github.com/dashdogy/RTX40MFG-Unlock
- Yeni paket: https://github.com/dashdogy/RTX40MFG-Unlock/releases/tag/v1.4.1-hotfix.1
- Önceki paket: https://github.com/dashdogy/RTX40MFG-Unlock/releases/tag/v1.3.3-hotfix.2
- JSON for Modern C++ 3.12.0: https://github.com/nlohmann/json — MIT bildirimi: `third_party/LICENSE-json.txt`.
- NVIDIA NVAPI SDK: https://github.com/NVIDIA/nvapi — kullanılan commit `third_party/nvapi/SOURCE-COMMIT.txt`, MIT bildirimi `third_party/LICENSE-nvapi.txt`.

Bu uygulama bağımsız bir kurulum yöneticisidir; NVIDIA veya mod geliştiricisinin resmi uygulaması değildir.

