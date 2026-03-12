# DSP Education Stand

JUCE standalone application for learning and testing DSP ideas in a project-based workflow.

The app now combines:

- built-in signal sources and built-in DSP
- project-based user DSP editing in C++
- real compilation and hot reload of user DSP modules
- custom realtime controls
- audio device selection, channel selection, and practical routing
- preferred audio configuration reported by the DSP module through API
- stereo oscilloscope and IDE-like dark UI

The repository targets local developer builds on macOS and Windows. It is a learning tool and DSP playground, not a DAW or plugin host.

It is also, very explicitly, a vibe-coding product built so that future DSP experiments can rely less on vibe coding and more on a structured workflow.

---

## English

### What The App Does Now

DSP Education Stand is no longer a single-file mono DSP demo. The current application supports:

- project-based user DSP development with `.dspedu` archives
- multiple source types:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- built-in DSP processors:
  - bypass
  - gain
  - hard clip
  - one-pole lowpass
  - simple delay
- user DSP compile/load/hot reload with safe fallback to the previous valid module
- custom realtime controls in a separate popup window:
  - knobs
  - buttons
  - toggles
- audio device configuration in the UI:
  - input device
  - output device
  - sample rate
  - block size
  - enabled input channels
  - enabled output channels
  - logical DSP input/output routing
- preferred sample rate, block size, and channel count coming from the user DSP API
- separate status reporting for:
  - preferred DSP values
  - requested values
  - actual active device values
  - manual user overrides
  - warnings and fallbacks
- stereo oscilloscope in a modeless resizable popup window
- dark IDE-style main window with custom title bar, collapsible side panels, project navigator, compile log, and code font size control

### Main Workflow

Typical flow:

1. Create or open a `.dspedu` project.
2. Edit files in the code editor.
3. Set the exported processor class in `Processor Class`.
4. Open `Controls` and add knobs, buttons, or toggles.
5. Give each control a UI label and a `code name`.
6. Compile the project.
7. The application builds a platform-native dynamic module:
   - `.dylib` on macOS
   - `.dll` on Windows
8. On success, the new module is loaded and swapped in safely.
9. Preferred audio configuration from the module is applied automatically when possible.
10. The user may still override device settings manually from the UI.

### User DSP API

The app injects the SDK include and plugin entry wrapper automatically during staging.

Do not manually add:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

The current user DSP contract is based on:

- `getPreferredAudioConfig(...)`
- `prepare(const DspEduProcessSpec&)`
- `reset()`
- `process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)`

Minimal example:

```cpp
#include <algorithm>
#include <cmath>

class MyAudioProcessor
{
public:
    const char* getName() const
    {
        return "Stereo Oscillator";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.preferredInputChannels = 0;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = std::max(1.0, spec.sampleRate);
        reset();
    }

    void reset()
    {
        phaseLeft = 0.0f;
        phaseRight = 0.25f;
    }

    void process(const float* const* /*inputs*/,
                 float* const* outputs,
                 int /*numInputChannels*/,
                 int numOutputChannels,
                 int numSamples)
    {
        if (outputs == nullptr || numOutputChannels <= 0 || outputs[0] == nullptr)
            return;

        auto* left = outputs[0];
        auto* right = numOutputChannels > 1 && outputs[1] != nullptr ? outputs[1] : outputs[0];

        const float frequencyHz = 110.0f + 770.0f * controls.frequencyKnob;
        const float detuneHz = -4.0f + 8.0f * controls.detuneKnob;
        const float level = 0.15f + 0.85f * std::clamp(controls.levelKnob, 0.0f, 1.0f);

        const float leftStep = frequencyHz / static_cast<float>(sampleRate);
        const float rightStep = (frequencyHz + detuneHz) / static_cast<float>(sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            const float sampleL = std::sin(twoPi * phaseLeft) * level;
            const float sampleR = std::sin(twoPi * phaseRight) * level;

            left[i] = controls.muteToggle ? 0.0f : sampleL;

            if (numOutputChannels > 1 && right != nullptr)
                right[i] = controls.muteToggle ? 0.0f : sampleR;

            advancePhase(phaseLeft, leftStep);
            advancePhase(phaseRight, rightStep);
        }
    }

private:
    void advancePhase(float& phase, float delta) const
    {
        phase += delta;

        while (phase >= 1.0f)
            phase -= 1.0f;
    }

    static constexpr float twoPi = 6.28318530717958647692f;

    double sampleRate = 44100.0;
    float phaseLeft = 0.0f;
    float phaseRight = 0.0f;
};
```

### Realtime Controls

The `Controls` window lets you define custom runtime controls for the user DSP module.

Supported control types:

- `Knob`
- `Button`
- `Toggle`

Behavior:

- knobs output `0..1`
- buttons are momentary boolean controls
- toggles are latched boolean controls
- controls are shown as equal-sized tiles
- controls can be reordered
- double-click a tile to edit:
  - visible label
  - code name
  - control type

The generated control names are available in user DSP code as:

```cpp
controls.driveKnob
controls.fireButton
controls.bypassToggle
```

### Audio Device Model

The app distinguishes three layers:

- preferred values reported by the user DSP module
- requested values after project settings and user overrides are merged
- actual active values accepted by the audio backend/device

This matters for:

- sample rate
- block size
- input channel count
- output channel count
- channel routing

If the preferred DSP configuration is unsupported by the current device, the app:

- keeps running
- logs a warning
- shows a warning in the UI
- falls back to the nearest supported or otherwise safe value

### Project Files And Serialization

Projects are stored as `.dspedu` ZIP archives.

The archive stores:

- project tree and source files
- active file and processor class
- custom control definitions
- cached preferred audio configuration from the last successful user DSP module
- last known actual audio configuration
- user override flags
- selected input/output devices
- enabled input/output channels
- routing state

### Build Requirements

Common:

- CMake 3.22 or newer
- local JUCE checkout provided through `JUCE_DIR`

Windows:

- Windows
- Ninja
- Visual Studio Build Tools with C++ workload

macOS:

- macOS
- Xcode or Xcode Command Line Tools
- Apple clang available through `xcrun`

Projucer is not used.

### Build

#### macOS

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --target DspEducationStand -j
```

Using environment variable:

```bash
export JUCE_DIR=/path/to/JUCE
cmake -S . -B build
cmake --build build --target DspEducationStand -j
```

#### Windows

```powershell
cmd /c scripts\build_windows.cmd build
```

If needed:

```powershell
$env:JUCE_DIR = "C:\Path\To\JUCE"
cmd /c scripts\build_windows.cmd build
```

### Tests

Optional test build:

```bash
cmake -S . -B build-tests -DDSP_EDU_BUILD_TESTS=ON -DJUCE_DIR=/path/to/JUCE
cmake --build build-tests --target DspEducationStandTests -j
./build-tests/DspEducationStandTests_artefacts/DspEducationStandTests
```

On Windows, run the test executable from the corresponding `build-tests` artefacts directory.

### Run

macOS example:

```bash
open "build/DspEducationStand_artefacts/DSP Education Stand.app"
```

Windows example:

```powershell
& ".\build\DspEducationStand_artefacts\Release\DSP Education Stand.exe"
```

The exact artefacts path may vary depending on generator and build configuration.

### Repository Layout

```text
src/
  app/         Application shell, main window, main component
  audio/       Audio engine, device config, generators, WAV source
  dsp/         Built-in DSP processors
  presets/     Built-in DSP preset manager
  ui/          Look-and-feel, oscilloscope, tool windows, controls UI
  userdsp/     Project manager, compiler, module host, project helpers
  util/        Small utility helpers
scripts/       Build scripts for Windows and user DSP modules
templates/     Default user DSP template
user_dsp_sdk/  Stable SDK header for user DSP modules
tests/         Optional regression tests
```

### Notes And Current Limitations

- The current user DSP audio API is practical mono/stereo, with `DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS = 2`.
- Routing is practical channel mapping through JUCE device abstraction, not a full arbitrary mixer matrix with per-route gain.
- The app is designed for local developer builds, not signed/notarized distribution.
- User DSP compilation depends on local toolchains:
  - MSVC on Windows
  - Apple clang on macOS
- Heavy work such as file I/O, project save/load, device reopen, and module compilation is kept off the audio callback path.

---

## Русский

Это ещё и вполне осознанный продукт вайбкодинга, сделанный ради того, чтобы дальше меньше вайбкодить и больше работать в структурированном DSP workflow.

### Что умеет приложение сейчас

DSP Education Stand больше не является только монофоническим single-file DSP-демо. В текущем состоянии приложение поддерживает:

- проектный user DSP workflow с архивами `.dspedu`
- несколько источников сигнала:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- встроенные DSP-процессоры:
  - bypass
  - gain
  - hard clip
  - one-pole lowpass
  - simple delay
- компиляцию, загрузку и hot reload пользовательского DSP-модуля с безопасным fallback на предыдущий рабочий модуль
- кастомные realtime-контроллеры в отдельном popup-окне:
  - кнобы
  - кнопки
  - тумблеры
- настройку аудиоустройства из UI:
  - input device
  - output device
  - sample rate
  - block size
  - активные input channels
  - активные output channels
  - routing логических DSP-входов и выходов
- preferred sample rate, block size и channel count, которые user DSP-модуль сообщает через API
- раздельные статусы для:
  - preferred значений из DSP
  - requested значений
  - actual значений устройства
  - ручных override пользователя
  - warnings и fallback-сценариев
- стерео-осциллограф в отдельном modeless resizable popup-окне
- тёмный IDE-like интерфейс с кастомным title bar, скрываемыми панелями, project navigator, compile log и управлением размером шрифта редактора

### Основной workflow

Типичный сценарий:

1. Создать или открыть `.dspedu` проект.
2. Редактировать файлы в code editor.
3. Указать экспортируемый класс в поле `Processor Class`.
4. Открыть `Controls` и добавить кнобы, кнопки или тумблеры.
5. Задать каждому контроллеру видимое имя и `code name`.
6. Скомпилировать проект.
7. Приложение соберёт платформенный динамический модуль:
   - `.dylib` на macOS
   - `.dll` на Windows
8. При успехе новый модуль будет безопасно загружен и активирован.
9. Preferred audio configuration из модуля будет автоматически применена, если устройство это поддерживает.
10. Пользователь всё равно может вручную переопределить device settings через UI.

### User DSP API

Во время staging приложение само добавляет SDK include и plugin entry wrapper.

Не нужно вручную писать:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

Текущий контракт user DSP основан на:

- `getPreferredAudioConfig(...)`
- `prepare(const DspEduProcessSpec&)`
- `reset()`
- `process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)`

Минимальный пример приведён в английской секции выше. Он соответствует текущему `v3` API.

### Realtime Controls

Окно `Controls` позволяет создавать кастомные runtime-контроллеры для пользовательского DSP-модуля.

Поддерживаются:

- `Knob`
- `Button`
- `Toggle`

Поведение:

- кноб даёт значение `0..1`
- кнопка работает как momentary boolean
- toggle хранит состояние как bool
- все контроллеры отображаются плитками одинакового размера
- контроллеры можно переставлять
- двойной клик по плитке открывает редактирование:
  - видимого `label`
  - `code name`
  - типа контроллера

В коде контроллеры доступны так:

```cpp
controls.driveKnob
controls.fireButton
controls.bypassToggle
```

### Аудиоустройства и routing

Приложение различает три слоя:

- preferred значения, которые пришли из user DSP API
- requested значения после объединения project settings и user overrides
- actual значения, которые реально приняло устройство и backend

Это используется для:

- sample rate
- block size
- input channel count
- output channel count
- routing

Если preferred configuration не поддерживается текущим устройством, приложение:

- не падает
- пишет warning в лог
- показывает предупреждение в UI
- выбирает ближайшее поддерживаемое или безопасное fallback-значение

### Проекты и сериализация

Проекты сохраняются как ZIP-архивы `.dspedu`.

В архиве сохраняются:

- дерево проекта и исходники
- active file и processor class
- описания кастомных контроллеров
- cached preferred audio configuration от последнего успешно загруженного user DSP-модуля
- last known actual audio configuration
- флаги user override
- выбранные input/output devices
- активные input/output channels
- routing state

### Требования к сборке

Общие:

- CMake 3.22 или новее
- локальный checkout JUCE, переданный через `JUCE_DIR`

Windows:

- Windows
- Ninja
- Visual Studio Build Tools с C++ workload

macOS:

- macOS
- Xcode или Xcode Command Line Tools
- Apple clang, доступный через `xcrun`

Projucer не используется.

### Сборка

#### macOS

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --target DspEducationStand -j
```

Через переменную окружения:

```bash
export JUCE_DIR=/path/to/JUCE
cmake -S . -B build
cmake --build build --target DspEducationStand -j
```

#### Windows

```powershell
cmd /c scripts\build_windows.cmd build
```

При необходимости:

```powershell
$env:JUCE_DIR = "C:\Path\To\JUCE"
cmd /c scripts\build_windows.cmd build
```

### Тесты

Опциональная сборка тестов:

```bash
cmake -S . -B build-tests -DDSP_EDU_BUILD_TESTS=ON -DJUCE_DIR=/path/to/JUCE
cmake --build build-tests --target DspEducationStandTests -j
./build-tests/DspEducationStandTests_artefacts/DspEducationStandTests
```

На Windows тестовый `.exe` нужно запускать из соответствующего каталога `build-tests`.

### Запуск

Пример для macOS:

```bash
open "build/DspEducationStand_artefacts/DSP Education Stand.app"
```

Пример для Windows:

```powershell
& ".\build\DspEducationStand_artefacts\Release\DSP Education Stand.exe"
```

Точный путь к artefacts может отличаться в зависимости от генератора и build configuration.

### Структура репозитория

```text
src/
  app/         Каркас приложения, главное окно, главный компонент
  audio/       Audio engine, device config, генераторы, WAV source
  dsp/         Встроенные DSP-процессоры
  presets/     Менеджер пресетов built-in DSP
  ui/          Look-and-feel, oscilloscope, tool windows, controls UI
  userdsp/     Project manager, компилятор, host модулей, project helpers
  util/        Небольшие утилиты
scripts/       Скрипты сборки для Windows и user DSP-модулей
templates/     Дефолтный шаблон user DSP
user_dsp_sdk/  Стабильный SDK header для пользовательских DSP-модулей
tests/         Опциональные regression tests
```

### Ограничения и заметки

- Текущий user DSP audio API рассчитан на практичный mono/stereo и ограничен `DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS = 2`.
- Routing реализован как практичное channel mapping поверх JUCE device abstraction, а не как произвольная матрица микширования с gain на каждую связь.
- Приложение ориентировано на локальные developer build, а не на подписанный и notarized distribution.
- Компиляция user DSP зависит от локального toolchain:
  - MSVC на Windows
  - Apple clang на macOS
- Тяжёлая работа, включая file I/O, save/load проекта, reopen device и compilation user DSP-модуля, вынесена из audio callback path.
