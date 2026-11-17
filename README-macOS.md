# Genny v1.54 — порт VST-плагина под macOS

Порт открытого VST-инструмента **GENNY** (эмулятор YM2612 + SN76489 из Sega Mega Drive)
с Windows на macOS. Оригинал: <https://github.com/superjoebob/genny> (Windows-only, Visual Studio + Win32).

## Что получилось

* Готовый VST2-бандл: `build/dist/Genny.vst`, universal (**arm64 + x86_64**), ad-hoc подпись.
* Звук проверен: тестовый хост грузит плагин, отправляет MIDI-ноту 60, рендерит 0.7 с —
  пик 0.5155, RMS 0.1393, одинаково на обеих архитектурах.
* GUI проверен: редактор 954×602 открывается в NSView и полностью отрисовывается
  (VSTGUI 4.10 + Cocoa-слой), включая банк пресетов из ресурсов.
* Банк расширен: 180 пресетов + 85 автоматически сгенерированных вариантов = **265**.
  Генератор — `tools/bank_gen.cpp` (собирается целью `genny_bankgen`), работает через
  код самого плагина, поэтому формат банка гарантированно совместим.
* Листание пресетов: стрелки вверх/вниз в панели PRESETS переключают пресет,
  колесо мыши прокручивает список (раньше стрелки только прокручивали вид).

## Установка

Установка по умолчанию — системная папка VST (её и сканирует FL):

```
/Library/Audio/Plug-Ins/VST/Genny.vst
```

Системная папка (`/Library/Audio/Plug-Ins/VST`) требует прав администратора:

```bash
sudo cp -R build/dist/Genny.vst /Library/Audio/Plug-Ins/VST/
```

### Включение в FL Studio

FL Studio 20 на macOS сканирует только **системную** папку `/Library/Audio/Plug-Ins/VST`
(пользовательскую `~/Library/Audio/Plug-Ins/VST` он не смотрит), поэтому плагин
установлен туда:

```bash
sudo cp -R build/dist/Genny.vst /Library/Audio/Plug-Ins/VST/
sudo chmod -R a+rX /Library/Audio/Plug-Ins/VST/Genny.vst
```

`chmod` обязателен: FL запускается от пользователя и должен прочитать `Info.plist`.

1. FL Studio → `Options` → `Manage plugins` → `Find more plugins` (или перезапустить FL).
2. Плагин появится в Browser → `Plugin database` → `Installed` → `Generators` → `VST` → **Genny**.
3. Если не появился — в том же окне `Manage plugins` добавить путь
   `/Library/Audio/Plug-Ins/VST` в список поиска VST2 и просканировать снова.

Важно: оригинал поддерживает только **44.1 кГц** — на других частотах инструмент
расстраивается (ограничение самого GENNY, не порта).

## Сборка

```bash
cd genny-mac
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build -j8
```

Требуется CMake ≥ 3.20 и Xcode Command Line Tools. Результат: `build/dist/Genny.vst`.

## Как устроен порт

| Слой | Решение |
|---|---|
| Точка входа VST2 | `VSTPluginMain` из вендоренного VST SDK 2.4 (`vstsdk2.4`) |
| Ядро DSP (YM2612/SN76489, ресемплеры, gzip) | Собрано без изменений логики, только совместимость типов |
| GUI | VSTGUI 4.10 из репозитория, подключён готовый Cocoa-слой (`lib/platform/mac`) |
| Ресурсы (PNG, банк пресетов, сэмплы) | Из Windows-ресурсов `.rc` в `Contents/Resources` (`tools/prepare_resources.py`) |
| Контекстные меню | Win32 `CreatePopupMenu/TrackPopupMenu` → нативный `NSMenu` (`platform/win32_menu_shim.h/.mm`) |
| Диалоги ввода | Win32 `DialogBox` (IDD_DIALOG1-3) → `NSAlert` + `NSTextField` (`platform/text_prompt.h`) |
| MessageBox, INI-чтение, `_wmkdir` | Реализованы на macOS в том же файле шима |

### Принципиальные правки для совместимости форматов

1. **`long` в MSVC — 32 бита.** Формат файлов Genny (банк пресетов `.gnb`, сэмплы)
   это учитывает: значения `long` пишутся 8 байтами, но значимы младшие 32 бита,
   а константы версий (`gdacVersion1..3`, `kVersionIndicator1`) на Windows усечены.
   На 64-битном `long` это давало сдвиг разбора: инструменты читались как «тип 50»
   вместо FM, и плагин молчал. Исправлено в `GennyData::readLong/writeLong`,
   `isGDACVersion`, `isVersionNumber` (`gennyLong32()`).
2. **Защита ресемплера.** При нулевой частоте сэмпла коэффициент ресемплинга
   вырождался в `step = 0` и цикл `Fir_Resampler_avail()` зависал навсегда.
   Добавлены проверки в `WaveData::reprocess` и `Fir_Resampler_time_ratio`.
3. **MSVC-расширения:** `__super` (включён `-fms-extensions`), `itoa`, `min/max`
   из `windows.h`, `__declspec`, `__stdcall` — заменены в `compat/genny_mac_compat.h`.

## Инструменты проверки

* `tools/vst_test_host.cpp` — минимальный VST2-хост: загрузка бандла, MIDI-нота,
  рендер аудио, отчёт о пике/RMS.
* `tools/vst_gui_test.mm` — открывает редактор в реальном NSView и сохраняет PNG-снимок.
* `tools/prepare_resources.py` — раскладка ресурсов из `.rc` в бандл.
* `tools/bank_gen.cpp` — генератор банка пресетов (оригиналы + варианты).
* `tools/apply_source_patches.py` — первичный набор правок исходников (для истории).

## Ограничения

* Только **VST2** (как в оригинале); AU/VST3 не делались.
* Портирована VST-версия. Отдельная FL-версия (`GennyFL`, работающая через Fruity SDK)
  не портирована — в FL используется именно VST-вариант.
* Вывод в MegaMIDI по последовательному порту не перенесён (в исходнике этот код
  закомментирован).
* Только 44.1 кГц, как и в оригинале.
