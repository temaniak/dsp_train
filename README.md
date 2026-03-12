# DSP Education Stand

Standalone JUCE desktop application for learning basic DSP concepts on Windows.

---

## English

### Overview

DSP Education Stand is a JUCE-based standalone desktop application that demonstrates a simple mono audio signal chain with:

- built-in signal generators
- built-in DSP processors
- WAV file playback
- oscilloscope visualization
- preset save/load for built-in DSP
- editable user DSP code in C++
- real DLL compilation and hot reload for user DSP modules

The project is intended as an MVP for DSP education, not as a production DAW plugin host.

### Main Features

- Signal sources: sine, white noise, impulse, WAV file
- Transport: start audio, stop audio
- Source controls: gain, sine frequency, sample rate info, block size info
- Built-in DSP: bypass, gain, hard clip, one-pole lowpass, simple delay
- User DSP mode: edit C++ code, compile to DLL, load compiled module, keep previous valid module on failure
- Visualization: oscilloscope
- Presets: save/load built-in DSP settings
- Import/export: save and load user DSP source code

### Requirements

- Windows
- CMake 3.22 or newer
- Ninja
- Visual Studio Build Tools with C++ workload
- Local JUCE checkout at `C:\Users\User\JUCE`

Projucer is not used.

### Build

From the project root:

```powershell
cmd /c scripts\build_windows.cmd build
```

The helper scripts automatically:

- find `vswhere.exe`
- locate `VsDevCmd.bat`
- initialize the MSVC environment
- configure CMake with Ninja
- build the application

If your JUCE path is different, set `JUCE_DIR` before running the script:

```powershell
$env:JUCE_DIR = "C:\Path\To\JUCE"
cmd /c scripts\build_windows.cmd build
```

### Run

After building, launch the generated executable from the build artefacts directory.

Example:

```powershell
& ".\build\DspEducationStand_artefacts\Debug\DSP Education Stand.exe"
```

Depending on your local generator and build mode, the exact subdirectory may differ.

### User DSP Workflow

The application supports real user DSP compilation.

How it works:

1. The editor text is written to a temporary `.cpp` file.
2. The app runs `scripts\build_user_dsp.cmd`.
3. The script initializes MSVC through `VsDevCmd.bat`.
4. The user module is compiled into a DLL with `cl /std:c++20 /LD`.
5. On success, the app loads the new DLL and swaps it in safely.
6. On failure, the previous valid DSP module stays active.

The user-facing API is intentionally simpler than the internal DLL ABI. The SDK adapter in [`user_dsp_sdk/UserDspApi.h`](user_dsp_sdk/UserDspApi.h) accepts both:

- the strict low-level API
- a simplified educational style such as `prepareToPlay(...)` and `processAudio(...)`

Minimal example:

```cpp
#include "UserDspApi.h"
#include <cmath>

class MyAudioProcessor
{
public:
    void prepareToPlay(float sampleRate)
    {
        sr = sampleRate;
    }

    void processAudio(float* buffer, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[i] = 0.2f * std::sin(phase);
            phase += 6.2831853f * 440.0f / sr;
        }
    }

private:
    float sr = 44100.0f;
    float phase = 0.0f;
};

DSP_EDU_DEFINE_SIMPLE_PLUGIN(MyAudioProcessor)
```

### Project Layout

```text
src/
  app/         Application and main window
  audio/       Audio engine, generators, WAV source
  dsp/         Built-in DSP processors
  presets/     Built-in DSP preset manager
  ui/          Oscilloscope components
  userdsp/     Code editor, compiler, DLL host
  util/        Utility helpers
scripts/       Windows configure/build scripts
templates/     Default user DSP template
user_dsp_sdk/  Stable SDK header for user DLLs
tests/         Minimal optional tests
```

### Notes and Limitations

- The current MVP uses mono internal processing.
- Heavy work such as file I/O and DLL compilation is kept off the audio thread.
- User DSP compilation requires a working MSVC Build Tools installation.
- Windows DLL locking is handled by versioned output naming instead of overwriting a loaded DLL.

---

## Русский

### Обзор

DSP Education Stand — это standalone desktop application на JUCE для изучения базовых DSP-концепций на Windows.

Приложение показывает простой монофонический audio pipeline с:

- встроенными генераторами сигнала
- встроенными DSP-алгоритмами
- воспроизведением WAV-файлов
- осциллографом
- пресетами для встроенного DSP
- редактированием пользовательского DSP-кода на C++
- реальной компиляцией пользовательского DSP в DLL и hot reload

Это учебный MVP, а не production-ready plugin host.

### Основные возможности

- Источники сигнала: sine, white noise, impulse, WAV file
- Транспорт: start audio, stop audio
- Параметры источника: gain, frequency для sine, информация о sample rate и block size
- Built-in DSP: bypass, gain, hard clip, one-pole lowpass, simple delay
- User DSP mode: редактирование C++ кода, компиляция в DLL, загрузка нового модуля, сохранение предыдущего рабочего модуля при ошибке
- Визуализация: oscilloscope
- Presets: сохранение и загрузка настроек встроенного DSP
- Import/export: сохранение и загрузка пользовательского DSP-кода

### Требования

- Windows
- CMake 3.22 или новее
- Ninja
- Visual Studio Build Tools с C++ workload
- Локальный JUCE по пути `C:\Users\User\JUCE`

Projucer не используется.

### Сборка

Из корня проекта:

```powershell
cmd /c scripts\build_windows.cmd build
```

Скрипты автоматически:

- находят `vswhere.exe`
- находят `VsDevCmd.bat`
- поднимают MSVC environment
- конфигурируют CMake через Ninja
- собирают приложение

Если JUCE лежит в другом месте, задай `JUCE_DIR` перед запуском:

```powershell
$env:JUCE_DIR = "C:\Path\To\JUCE"
cmd /c scripts\build_windows.cmd build
```

### Запуск

После сборки запусти exe из каталога build artefacts.

Пример:

```powershell
& ".\build\DspEducationStand_artefacts\Debug\DSP Education Stand.exe"
```

Точный подкаталог может отличаться в зависимости от локального режима сборки.

### Пользовательский DSP Workflow

В приложении реализована настоящая компиляция пользовательского DSP.

Как это работает:

1. Текст из редактора сохраняется во временный `.cpp` файл.
2. Приложение запускает `scripts\build_user_dsp.cmd`.
3. Скрипт поднимает MSVC через `VsDevCmd.bat`.
4. Пользовательский модуль компилируется в DLL через `cl /std:c++20 /LD`.
5. При успехе приложение загружает новую DLL и безопасно переключает обработку на неё.
6. При ошибке предыдущий валидный DSP-модуль остаётся активным.

Пользовательский API намеренно сделан проще, чем внутренний DLL ABI. Адаптер в [`user_dsp_sdk/UserDspApi.h`](user_dsp_sdk/UserDspApi.h) поддерживает:

- строгий низкоуровневый API
- упрощённый учебный стиль, например `prepareToPlay(...)` и `processAudio(...)`

Минимальный пример:

```cpp
#include "UserDspApi.h"
#include <cmath>

class MyAudioProcessor
{
public:
    void prepareToPlay(float sampleRate)
    {
        sr = sampleRate;
    }

    void processAudio(float* buffer, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[i] = 0.2f * std::sin(phase);
            phase += 6.2831853f * 440.0f / sr;
        }
    }

private:
    float sr = 44100.0f;
    float phase = 0.0f;
};

DSP_EDU_DEFINE_SIMPLE_PLUGIN(MyAudioProcessor)
```

### Структура проекта

```text
src/
  app/         Приложение и главное окно
  audio/       Audio engine, генераторы, WAV source
  dsp/         Встроенные DSP processors
  presets/     Менеджер пресетов built-in DSP
  ui/          Компоненты осциллографа
  userdsp/     Редактор кода, компилятор, DLL host
  util/        Вспомогательные утилиты
scripts/       Windows configure/build scripts
templates/     Дефолтный шаблон user DSP
user_dsp_sdk/  Стабильный SDK header для пользовательских DLL
tests/         Минимальные опциональные тесты
```

### Ограничения и заметки

- Внутренняя обработка в текущем MVP монофоническая.
- Тяжёлая работа, включая file I/O и DLL compilation, вынесена из audio thread.
- Для пользовательской компиляции DSP нужен установленный MSVC Build Tools.
- Проблема Windows DLL locking решается через versioned naming, а не через перезапись загруженной DLL.
