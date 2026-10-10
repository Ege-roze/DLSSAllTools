# DLSS All Tools — devam kaydı

## Amaç
C++20 ve Windows masaüstü arayüzü ile RTXMFG kurulum yöneticisi.
Hedef proje: C:\Users\Ege\Desktop\dlssalltools

## İlk sürüm kapsamı
- Steam ve Epic kütüphanelerini tara; elle gerçek oyun EXE'si ekle.
- GitHub yayın paketini indir, SHA-256 doğrula ve RTXMFG.dll hazırla.
- EXE'nin yüklediği uygun boş proxy adını tespit et; bilinmeyen oyunda otomatik desteği garanti etme.
- Dosyaların üzerine yazmadan kur; işlem günlüğünü önceden kalıcı kaydet.
- Yalnızca kayıttaki hash ile eşleşen kendi DLL'sini kaldır.
- Oyun çalışırken değişiklik yapma. Ayarları koru.
- Türkçe arayüz, kalıcı oyun listesi ve kurulum kayıtları.
- Gerçek oyunlara dokunmadan izole klasörlerde kur/kaldır/çakışma testleri.

## Durum
2026-10-08: Boş Visual Studio 2026 C++ projesi bulundu. v145 araçları kurulu.
Gönderilen kaynak ZIP, genel RTXMFG.dll içermiyor. Resmi sürüm kullanılacak.
Güncel durum (2026-10-09): v0.2 tamamlandı. Ayrıntılar son kontrol noktasında.

## Çalışma düzeni
Kod önce çalışma alanındaki work/dlssalltools içinde hazırlanıp derlenir.
Kontrol noktalarında masaüstündeki asıl projeye kopyalanır; .vs ve .user korunur.
Başlangıç proje dosyası .checkpoints/initial içinde saklanır.
Kullanıcının oyunlarına ancak uygulamada Kur seçildiğinde yazılır.

## Kontrol noktası 1 — kaynak kod hazır
- Türkçe Win32 arayüzü ve DPI desteği yazıldı.
- Steam/Epic tarayıcı, PE import analizi, sürüm sabitli indirici yazıldı.
- SHA-256 doğrulaması, çakışma koruması, önceden kalıcı kurulum kaydı ve hash kontrollü kaldırma yazıldı.
- 26 izole test senaryosu eklendi; henüz çalıştırılmadı.
- Release x64 derlemesi sürüyor. Sonraki adım derleme hatalarını düzeltip testleri çalıştırmak.
- GOG otomatik taraması ve Bink DLL zincirlemesi ilk sürüme dahil değil; elle EXE ekleme var.

## Kontrol noktası 2 — çalışan derleme
- Release x64 başarıyla derlendi.
- 26/26 izole kurulum/kaldırma testi geçti (tests/results-full/test-results.txt).
- Uygulamanın kendi indiricisi gerçek GitHub yayınıyla çalıştı; ZIP ve DLL SHA-256 doğrulandı.
- Gerçek taramada 44 Steam/Epic kütüphane öğesi ve RTX 4060 Laptop GPU bulundu.
- Cyberpunk 2077 gerçek EXE konumu ve boş version.dll kurulum yolu bulundu; oyuna kurulum yapılmadı.
- Tarama sırasında gereksiz büyük EXE okumaları veri eşlemesiyle azaltıldı; launcher/helper filtreleri geliştirildi.
- Sonraki adım: güncel sürümün regresyon testi, GUI doğrulama, proje ve taşınabilir EXE teslimi.
- Sandbox içindeki MSVC PDB hizmeti başarısız oldu; normal yetkili derleme başarılı. Visual Studio kurulumunda değişiklik yapılmadı.

## Kontrol noktası 3 — v0.1 tamamlandı
- Release/x64 ve Debug/x64 derlemeleri başarılı.
- Güncel kaynakta 27/27 test başarılı: tests/results-final/test-results.txt.
- Resmi paket indirme hem uygulama içinden hem başsız testten doğrulandı.
- Arayüz gerçek pencerede kontrol edildi: liste, paket hazır durumu, Cyberpunk seçimi ve Kur düğmesinin etkinleşmesi çalışıyor.
- Koyu tema başlık/düğme köşeleri düzeltildi; FG bulunamayan oyunlar ayrı etiketleniyor.
- EXE'si silinmiş oyunun kurulum kaydı arayüzde de kaldırılabilir durumda tutuluyor.
- Tarama: 44 kütüphane öğesi, yaklaşık 10.6 saniye; Cyberpunk 2077 ve GTA V Enhanced için EXE ve version.dll yolu tespit edildi.
- Kullanıcının oyunlarına hiçbir kurulum yapılmadı. Modun gerçek oyun içi görüntü/performans doğrulaması yapılmadı.

## Devam edecek geliştirmeler
1. Kullanıcının seçtiği oyunda gerçek kurulum ve oyun içi MFG doğrulaması.
2. Oyun arama/filtreleme, GOG kütüphanesi desteği ve daha geniş oyun profilleri.
3. Kullanıcı isterse eski mod kurulumunu devralma ve Bink yedekleme/geri yükleme desteği.
4. Yeni upstream sürümleri için hash'leri doğrulayarak kontrollü güncelleme desteği.

## Önemli kaynaklar
- src/core.cpp: tarama, PE veri analizi, paket indirme/doğrulama, kurulum/kaldırma.
- src/main.cpp: Türkçe Windows arayüzü ve iş parçacığı yönetimi.
- src/tests.cpp: gerçek oyunlara dokunmayan 27 test.
- README.md: kullanım ve teknik sınırlar.

## Kontrol noktası 4 — v0.2 sürüm seçimi (devam ediyor)
- Kullanıcı Cyberpunk'ta 2x sorunsuz, 3x/4x titreme bildirdi. Yeni sürüm regresyonu henüz kanıtlanmadı.
- v1.3.3 Hotfix 2 resmi bütün paket indirildi; SHA256SUMS.txt ile ZIP ve DLL doğrulandı. Kaynak ZIP'teki eski ASI kullanılmıyor.
- İki sabit sürüm için ayrı önbellek, arayüzde seçim ve kayıtlı seçimin geri yüklenmesi eklendi. İlk açılış varsayılanı v1.3.3-hotfix.2.
- Kurulum kaydı sürüm/hash eşleşmesiyle doğrulanıyor; kaldırma seçili paket yerine kurulu paketin kaydını kullanıyor.
- Sürüm değişimi: önce Kaldır, ardından Kur. Kurulu dosyanın üzerine başka sürüm yazılmaz.
- Önceki kaynak yedeği: ../checkpoint-04-before-versions.zip (çalışma alanındaki work klasöründe).
- Yeni sürümler arası testler yazıldı. Derleme, testler, arayüz kontrolü ve masaüstüne aktarım henüz tamamlanmadı.


## Kontrol noktası 5 — v0.2 tamamlandı (2026-10-09)
- Release/x64 ve Debug/x64 derlemeleri başarılı, her birinde 38/38 test başarılı.
- İki resmi paket uygulamanın kendi indiricisiyle indirildi; ZIP ve DLL SHA-256 doğrulandı.
- Gerçek pencerede v1.3.3 → v1.4.1 → v1.3.3 seçimi, ayrı paket hazır durumu ve Cyberpunk Kur düğmesi doğrulandı.
- Kullanıcının uygulama önbelleğinde iki paket hazır; preferences.json içinde v1.3.3-hotfix.2 seçili.
- Cyberpunk oyun klasörüne bu çalışmada yazılmadı. Kurulum kaydı/version.dll bulunmadığından arayüzde Kur kullanılabilir. Mevcut ReShade dxgi.dll korundu.
- Oyunda deneme sonraki adım: Cyberpunk seçili, 1.3.3 Hotfix 2 (önceki), Kur. Titreme düzelmesi henüz doğrulanmadı.
- Önceki sürüme dönüş de Kaldır → sürüm seç → Kur ile yapılır. Mevcut mod ayarları korunur.
- İlerlemenin ve geri alma kopyalarının masaüstü .checkpoints klasörüne aktarımı yapılır; Visual Studio kişisel ayarlarına dokunulmaz.
- Release EXE SHA-256: 09486D315C8433FCDD2D3DC95F391B0CF47BEA1A9CC1E7116E95882D021EC035.
- Teslim: outputs/DLSSAllTools-v0.2 ve outputs/DLSSAllTools-v0.2-portable.zip.

## Kontrol noktası 6 — v0.3 NVIDIA Preset B çözümü (devam ediyor)
- GitHub issue #6 / issuecomment-5558240358 API'den doğrulandı: NVIDIA profilinde preset=2 ve override enable=1 birlikte gerekli; yalnızca mod JSON'u yeterli değil.
- Resmi NVIDIA NVAPI SDK başlıkları sabit commit'ten third_party/nvapi içine alındı. Kaynak SHA SOURCE-COMMIT.txt içinde.
- Sistem sürücüsünden NVAPI yükleyen, tam EXE yoluyla oyuna özel profili bulan, sadece iki FG ayarını değiştirebilen katman yazıldı.
- Ayar öncesi kalıcı yedek, sonradan okuma doğrulaması, önceki özel değeri/varsayılan mirasını geri alma ve çakışma denetimi eklendi.
- Arayüzde isteğe bağlı sorun giderme kutusu hazırlanıyor. Canlı NVIDIA profiline yazma yapılmadı.
- Sonraki adım: derleme, gerçek profili salt okunur doğrulama, izole hatalı/yarım işlem testleri, UI ve teslim.

## Kontrol noktası 7 — v0.3 testleri tamamlandı, UI kontrolü sürüyor
- Release ve Debug x64 derlendi, ikisinde de 58/58 test geçti.
- 20 yeni test: iki ayarın birlikte yazılması, önce yedekleme, özel/varsayılan değerlerin geri alınması, yarım kayıt, yetki hatası, dışarıdan değişen ayar, genel/paylaşılan profil engeli ve çalışan oyun engeli.
- Gerçek NVIDIA API yalnızca okuma ile doğrulandı. Cyberpunk 2077 profili cyberpunk2077.exe + cplauncher.exe içeriyor. İki FG override ayarı da profilde kayıtlı değil.
- Başlatıcı, yalnızca bu bilinen Cyberpunk profili için güvenli kapsam olarak destekleniyor. Başka programla paylaşılan profiller reddediliyor.
- Henüz canlı NVIDIA profiline yazılmadı. İşaretleme eylemi kullanıcıya bırakılacak.
- NVAPI kaynak commit: 70d337db9186e968eab622f7e786de7e437faf3d.

## Kontrol noktası 8 — v0.3 teslimi
- Kod, Release ve Debug derlemeleri tamamlandı; 58 test iki yapılandırmada da geçti.
- Arayüz kontrolü kullanıcı fiziksel Escape tuşuyla Computer Use'u durdurduğu için tamamlanmadı. Bu turda başka pencere etkileşimi yapılmadı.
- Gerçek sürücüye ayar yazılmadı; etkinleştirme kullanıcı işaretlediğinde gerçekleşir. Gerçek oyun içinde çözümün etkisi henüz doğrulanmadı.
- Kullanım: oyunu kapat → oyunu seç → 3x / 4x görüntü sorunu… → Preset B çözümünü etkinleştir. Geri alma aynı kutunun işaretini kaldırır.
- Sürücü ayarı mod kaldırmadan bağımsızdır. profile-fixes yedek klasörünü koruyun.
- Ek kaynaklar: src/driver.cpp (resmi NVAPI), src/preset.cpp (yedek/geri alma), src/preset.hpp; kaynak SDK third_party/nvapi.
- Release EXE SHA-256: 47B7A0B56BE764A24BEC2B4DF1BD9BCD419E75EC5AABFF29A0D9CA427BE00CEC
- Teslim: outputs/DLSSAllTools-v0.3 ve DLSSAllTools-v0.3-portable.zip.

## Kontrol noktası 9 — v0.4 DLSS 5 entegrasyonu
- Kullanıcı Preset B sonrası Cyberpunk oyun içinde kusursuz çalıştığını doğruladı.
- MFG ve DLSS 5 bağımsız kurulumlar. DLSS 5 için ReShade 6.8 Add-on, RenoDX 6.5.3, SR 310.9.1, RTX40/RTX50 NR 310.8 paketleri sabitlendi.
- Kaynak: DLSS5-Swapper commit 9fb0b7c3563d350aa46eaa01d3e5ca4cbab4a7cb; x64 DX11/12 native + Feeder 1.17.0 yolları.
- SHA-256 sabit katalog, kalıcı çok dosyalı yedek/geri alma ve ayrı DLSS 5 penceresi yazıldı. Derleme ve testler henüz tamamlanmadı.
- Önceki kaynak yedeği work/checkpoint-09-before-dlss5.zip. Oyun dosyaları değiştirilmedi.

## Kontrol noktası 10 — v0.4 doğrulama
- 74/74 birim/işlem testi Release ve Debug'da geçti. 7/7 gerçek paket kur/kaldır entegrasyon testi de geçti.
- Uygulamanın kendi indiricisiyle RTX40 ve RTX50 paketleri başarıyla indirildi, arşiv ve 34 dosya kaydı doğrulandı.
- Gerçek UI'da Cyberpunk seçildi, ayrı DLSS5 penceresi açıldı, RTX40 ve mevcut-DLSS otomatik seçimleri doğrulandı. Kur düğmesine basılmadı.
- Son denetimde Vulkan EXE yanında DLSS bulunmasının DX11/12 olarak algılanması engellendi; son derleme/test tekrarı bekleniyor.
- Bu turda hiçbir gerçek oyuna DLL kopyalanmadı ve NVIDIA profili değiştirilmedi. Oyun içi sonuç kullanıcı testine bırakıldı.
- Masaüstüne kopyalama ve v0.4 portable çıktı hazırlığı kaldı.

## Kontrol noktası 11 — v0.4 teslim
- Son Release ve Debug derlemeleri: 0 hata / 0 uyarı. Son API kontrolü dahil gerçek paket testleri her ikisinde 7/7 geçti. Önceki 74 işlem testi de her ikisinde geçti.
- DLSS 5 kendi penceresinden isteğe bağlı indir/kur/geri al. MFG ana penceredeki ayrı düğmelerle yönetilir.
- RTX40 NR paketi 310.8.0-RTX40 olarak sabitlendi; RTX50 seçeneği ayrı runtime indirir. Lecram gibi RTX50-only paket RTX40 yerine kullanılmaz.
- Gerçek oyunlara kurulum veya sürücüye ayar yazma yapılmadı. Kullanıcı oyun içinde değerlendirecek.
- Release EXE SHA-256: EF75947AC10CA6BB2F3D6DD5F2C85B9E8B94AACE5859E53BD73338FA683FDC1C
- Teslim: outputs/DLSSAllTools-v0.4, outputs/DLSSAllTools-v0.4-portable.zip. Masaüstü proje, kaynak, belgeler ve EXE aynı sürüme güncellenir.
- Kaynaklar ve kapsam docs/DLSS5-SOURCES.md içinde. Sonraki olası çalışma kullanıcı testine göre oyun profilleri ve daha geniş API desteği.

## Kontrol noktası 12 — v0.5 seri desteği, araştırma ve uygulama
- Önceki kaynak yedeği: work/checkpoint-12-before-gpu-series.zip. Masaüstü dosyalarının başlangıç hash listesi ayrı kaydedildi.
- RTX20/30 DLSS5: 310.8.SF-v2 ZIP ve DLL hashleri upstream katalogla eşleşti; ortak FP16 runtime. Yerel RTX20/30 donanım testi yapılmadı. RTX40/50 paketleri korunuyor.
- RTX20/30 MFG: sdli1995/dlssg_for_sm86 0.3.5, commit 9621db573e07ed54f50c15bbb585ed9a7bdfac28. README 2080 Ti/3070/3080 Ti testlerini belgeliyor. 310.9 runtime, varsayılan 4x, Optimized=1.
- Dört farklı proxy DLL kendi sabit hashleriyle indiriliyor; oyuna sadece uygun boş utility proxy ve INI kuruluyor. DXGI'ye kurulmayarak DLSS5/ReShade yolu korunuyor.
- Ortak işlem katmanı ayrı dosya izin listeleri ve ayrı kurulum kayıtlarıyla genişletildi. RTX40 eski kayıtları desteklenmeye devam ediyor.
- Ana MFG ekranı ve DLSS5 penceresinde RTX20/30/40/50 seçimi; otomatik GPU algılama, bilinmeyen kartta varsayılan RTX40 seçmeme, farklı seri için yerel kurulumu engelleme eklendi.
- RTX50 MFG: oyun içi yerel destek kullanılır, kilit açma paketi indirilmez.
- Henüz derleme/test/UI ve masaüstüne teslim yapılmadı. Gerçek oyunlara yazılmadı.


## Kontrol noktası 13 — v0.5 testleri ve arayüz doğrulaması
- Release ve Debug derlemeleri 0 hata / 0 uyarı. 83/83 işlem testi ve 12/12 yeni gerçek paket testi her ikisinde geçti. 7 DLSS5 regresyon testi Debug'da geçti, Release kontrolü sürüyor.
- Kısıtlı ortamda hard-link testinin oluşturma adımı engellendi; aynı izole testler izinli çalıştırmada eksiksiz geçti. Oyun veya gerçek sürücü ayarı değiştirilmedi.
- Uygulamanın kendi indiricisi SF-v2 ve SM86 paketlerini indirdi; SHA-256 kontrolleri başarılı. Dört seri için doğru NR seçimi, MFG ile birlikte kurulum ve iki farklı kaldırma sırası test edildi.
- UI: RTX4060 Laptop otomatik RTX40 seçiliyor; RTX20 seçince SM86 0.3.5 listeleniyor, yanlış seri kurulumu ve RTX40 DLL içe aktarma kapanıyor. Paket indirme kullanılabilir.
- Kaynak kontrol noktası: work/checkpoint-13-v0.5-source.zip. Son dokümantasyon ve masaüstü teslimi devam ediyor.

## Kontrol noktası 14 — v0.5 son doğrulama ve paketleme
- Release ve Debug: 83/83 işlem testi + 12/12 GPU paket testi + 7/7 DLSS5 regresyon testi = yapılandırma başına 102 başarılı test.
- Gerçek UI'da RTX20 seçimi SM86 0.3.5'e, DLSS5 penceresinde SF-v2'ye yönlendi. 4060 Laptop üzerinde farklı seri için kurulumun kapalı olduğu ve açıklama görüldüğü doğrulandı.
- DLSS5 penceresinin yüksekliği ekrana sığması için 354 dialog birimine indirildi; iki derleme de hatasız. Bu son yerleşimin görsel tekrarı, uygulama penceresi kapandığı için tamamlanmadı. İşlev testleriyle kaynak seçimi ve kur/kaldır doğrulaması tam.
- Sabit MFG commit'i /commits/0.3.5 üzerinden de doğrulandı: 9621db573e07ed54f50c15bbb585ed9a7bdfac28.
- Son Release EXE SHA-256: 2ed2278e726d5ee9d0287055f58183dedc29ff7cdd1cc54a77e144e592bf9ede.
- Belgeler: README.md, docs/GPU-PACKAGES.md, docs/DLSS5-SOURCES.md; test kayıtları docs/TEST-RESULTS.txt.
- work/checkpoint-14-v0.5-source.zip ve outputs/DLSSAllTools-v0.5-portable.zip hazırlandı. Masaüstüne aktarma son adım.

## Kontrol noktası 15 — v0.5 teslim edildi
- Masaüstündeki dlssalltools proje kaynakları, belgeler, Release/Debug çıktıları ve kök DLSSAllTools.exe güncellendi; kaynak ve EXE hashleri çalışma kopyasıyla eşleşti.
- Önceki v0.4 EXE `.checkpoints/DLSSAllTools-before-v0.5.exe` olarak saklandı. 12/13/14 kaynak kontrol noktaları da masaüstüne kopyalandı.
- RTX20/30 SF-v2 NR runtime ve SM86 MFG paketi uygulamanın yerel önbelleğine doğrulanarak eklendi. Yeni kullanıcılar aynı paketleri uygulama içinden indirir; portable EXE'ye üçüncü taraf DLL gömülmez.
- Son paket: outputs/DLSSAllTools-v0.5-portable.zip. Gerçek oyun kurulumları ve NVIDIA profil ayarları bu çalışmada değiştirilmedi.
- 20/30 üzerinde gerçek oyun testi hâlâ kullanıcı donanımı gerektirir. Paket ve kurulum testleri bu donanım testinin yerine geçmez.


## Kontrol noktası 16–17 — v0.6 dil desteği
- v0.5 kaynak yedeği work/checkpoint-16-before-languages.zip; masaüstü başlangıç dosya hashleri ayrı saklandı.
- Türkçe, English ve 简体中文 (Basitleştirilmiş Çince), ana pencerenin sağ üstünden anlık seçiliyor.
- 307 metin; ana pencere, paket/kurulum durumları, hata mesajları, Preset B ve DLSS5 pencereleri çevrildi.
- Tercih ui-preferences.json içinde saklanır; bozuk veya bilinmeyen tercih Türkçeye döner. Mod tercihi, kütüphane ve oyun dosyaları ayrı kalır.
- Çince için Microsoft YaHei UI yazı tipi, derlenmiş katalog; çalışma sırasında çeviri indirilmez.
- İlk Release derlemesi başarılı. Kayıt/Unicode/tercih ayrımı testleri ve mevcut kurulum regresyonları ile görsel kontrol sürüyor.


## Kontrol noktası 18 — v0.6 doğrulama ve paket
- Release ve Debug: yapılandırma başına 114/114 test (12 dil + 83 işlem + 12 GPU paketi + 7 DLSS5 regresyonu), derlemeler 0 hata/uyarı.
- Gerçek pencerelerde Türkçe ana ekran, İngilizce/Çince ana ekran ve DLSS5/Preset B pencereleri kontrol edildi; metinler ve kapatma düğmeleri görünür. Dil geçişinde Cyberpunk seçimi ve 1.3.3 sürüm tercihi korundu.
- 307 çeviri EXE içinde. Dil tercihi, kayıt hatası, bozuk dosya, Unicode, kütüphane ve mod tercihi ayrımı testleri başarılı.
- Gerçek oyuna kurulum veya NVIDIA ayarı yazılmadı. Sadece durum/profil okundu. Modların kendi menüleri ve Windows'un sistem kontrolleri uygulama dil ayarından bağımsız.
- Release SHA-256: 729be639e4e303876e8358adafa82ac34a155784bde145e329b46b27808e36ff
- README ve docs/LOCALIZATION.md güncellendi; v0.6 portable ZIP hazırlanıyor. Masaüstü aktarımı son adım.


## Kontrol noktası 19 — v0.6 teslim edildi
- Masaüstü proje, Release/Debug çıktıları ve kök DLSSAllTools.exe güncellendi; kaynaklar ve Release EXE hashleri doğrulandı.
- v0.5 EXE masaüstündeki .checkpoints/DLSSAllTools-before-v0.6.exe olarak korundu. 16/17/18 kaynak kontrol noktaları aynı klasöre kopyalandı.
- Gerçek uygulama kapatılıp açıldığında Çince tercihinin korunduğu da doğrulandı. Görsel test sonunda Türkçe geri seçildi.
- Teslim paketi outputs/DLSSAllTools-v0.6-portable.zip; EXE çevirileri içerir, ayrı dil dosyası gerektirmez.
