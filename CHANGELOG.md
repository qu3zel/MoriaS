# Changelog

## 1.1.0
- **Film Kalitesinde Durin'in Kapıları (Peter Jackson LOTR)**:
  - 4 oktavlı FBM gürültü ve arduvaz damarlarıyla prosedürel Moria kaya dokusu ve ay ışığı vinyeti (32-bit DIB).
  - 3 katmanlı sinematik Ithildin ışıması (turkuaz dış aura, camgöbeği ışıma, beyaz-buz çekirdek).
  - Sütunlara S-kıvrımlarıyla sarılan ve ortadaki arayüzü çerçeveleyen lir formunda Telperion ve Laurelin ağaçları.
  - Saat yönünde kesintisiz akan dış Tengwar rün koridoru ve iç kemer yazıtı ("Im Narvi hain echant...").
  - Elmas dilimli Durin Tacı, çekiç-örs, 7 parıldayan iğne yıldız, 8 köşeli Fëanor Yıldızı ve dinamik "C/J" Celebrimbor/Narvi usta mührü.
- **Monitör Yenileme Hızında (60Hz / 144Hz / 240Hz) Ayarlar Animasyonu**:
  - DWM `WS_EX_LAYERED` gecikmesini önleyen donanımsal `COLORKEY` + RAM içi anlık kırpma mimarisiyle sıfır takılmalı açılış/kapanış.
  - Görev çubuğuna (Taskbar) kusursuz alt eşik sabitlemesi.
  - Arka plandaki taş dokusunu gösteren yarı saydam taş oyma butonlar.
  - Beyaz buton titremesini (flicker) engelleyen alt sınıf pencere prosedürü.
- **Düzeltmeler & İyileştirmeler**:
  - Bezier eğri örneklemesindeki tekillik ve dikey çizgi hataları giderildi.
  - Metin, ilerleme çubuğu ve butonların kapı motifleriyle çakışması önlendi.
  - MinGW ve MSVC için CMake derleme konfigürasyonu güncellendi (`gdiplus`, `comctl32`, `winmm`).

## 1.0.0
- Tepsi paneli (flyout) olarak yeniden tasarlandı: kapı kemeri biçimli, kayma/solma animasyonlu.
- Hazır süre düğmeleri (15/30/45/60/90), fare tekerleğiyle süre ayarı, ilerleme çubuğu, kapanış saati.
- Tepsi simgesi duruma göre değişir; tooltip'te kalan süre; sağ tık hızlı menüsü.
- Geri sayım uygulama yeniden başlasa da sürer (bilgisayar yeniden başlatıldıysa sürmez).
- Programatik Durin Kapısı süslemeleri: Elfçe yazıt şeridi, ağaçlar, yedi yıldız, taç, çekiç ve örs.
- Tek örnek, DPI desteği, küçük ekranlarda otomatik ölçekleme, Explorer yeniden başlarsa simgenin dönmesi.
- Düzeltmeler: GDI nesne sızıntısı, yanlış pencere boyutu, kaydırıcı titremesi, tırnaksız başlangıç yolu,
  `shutdown.exe` için tam yol, Enter ile yanlışlıkla iptal, yazı tipi yoksa yedek font.
