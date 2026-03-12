# DSP Education Stand

Cross-platform JUCE desktop application for learning basic DSP concepts on Windows and macOS.

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
- real user DSP module compilation and hot reload

The project is intended as an MVP for DSP education, not as a production DAW plugin host.

### Main Features

- Signal sources: sine, white noise, impulse, WAV file
- Transport: start audio, stop audio
- Source controls: gain, sine frequency, sample rate info, block size info
- Built-in DSP: bypass, gain, hard clip, one-pole lowpass, simple delay
- User DSP mode: edit a multi-file DSP project, compile a platform-native module, load the compiled module, keep the previous valid module on failure
- Visualization: oscilloscope
- Presets: save/load built-in DSP settings
- DSP projects: save/load `.dspedu` archives, navigate folders and files, import/export helper `.cpp` files

### Requirements

Common:

- CMake 3.22 or newer
- Local JUCE checkout provided through `JUCE_DIR`

Windows:

- Windows
- Ninja
- Visual Studio Build Tools with the C++ workload

macOS:

- macOS
- Xcode or Xcode Command Line Tools
- Apple clang available through `xcrun`

Projucer is not used.

### Build

#### Windows

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

#### macOS

From the project root:

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --target DspEducationStand -j
```

If you prefer using an environment variable:

```bash
export JUCE_DIR=/path/to/JUCE
cmake -S . -B build
cmake --build build --target DspEducationStand -j
```

### Run

After building, launch the generated application from the build artefacts directory.

Windows example:

```powershell
& ".\build\DspEducationStand_artefacts\Debug\DSP Education Stand.exe"
```

macOS example:

```bash
open "build/DspEducationStand_artefacts/DSP Education Stand.app"
```

Depending on your generator and build mode, the exact subdirectory may differ.

### User DSP Project Workflow

The application supports real user DSP compilation on both supported desktop platforms, and the editable unit is now a project rather than a single source buffer.

How it works:

1. `New Project` creates a default DSP project with `src/` and `include/`.
2. The Project Navigator on the right shows files and folders as a tree.
3. Projects are saved and loaded as `.dspedu` ZIP archives.
4. The editor still shows one file at a time, but compilation sees the whole project.
5. The Processor Class field tells the app which class to export from the project.
6. The app stages project files automatically and injects `#include "UserDspApi.h"` into translation units.
7. The app injects `DSP_EDU_DEFINE_SIMPLE_PLUGIN(ProcessorClass)` automatically during staging.
8. On Windows, `scripts/build_user_dsp.cmd` initializes MSVC and builds a `.dll`.
9. On macOS, `scripts/build_user_dsp.sh` invokes `xcrun clang++` and builds a `.dylib`.
10. On success, the app loads the new module and swaps it in safely.
11. On failure, the previous valid DSP module stays active.

The user-facing API stays class-based. The user writes processor classes, helper headers, and supporting `.cpp` files. The SDK wrapper remains internal.

Minimal example:

```cpp
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
```

Then enter `MyAudioProcessor` in the Processor Class field and compile the project.

Manual `#include "UserDspApi.h"` or manual `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)` in the editor is treated as an error, because the app injects both automatically.

Project navigator actions:

- Root or folder: `New File`, `New Folder`, `Import CPP`
- File or folder: `Rename`, `Move`, `Delete`
- File: `Export CPP`

Supported project source files in v1 are `.cpp`, `.h`, and `.hpp`.

### Project Layout

```text
src/
  app/         Application and main window
  audio/       Audio engine, generators, WAV source
  dsp/         Built-in DSP processors
  presets/     Built-in DSP preset manager
  ui/          Oscilloscope components
  userdsp/     Code editor, compiler, module host
  util/        Utility helpers
scripts/       Build scripts for Windows and user DSP modules
templates/     Default user DSP template
user_dsp_sdk/  Stable SDK header for user DSP modules
tests/         Minimal optional tests
```

### Notes and Limitations

- The current MVP uses mono internal processing.
- Heavy work such as file I/O and user module compilation is kept off the audio thread.
- macOS support in this repository targets local developer builds, not notarized distribution.
- Versioned module output naming avoids overwriting a loaded module during hot reload.

---

## Русский

### Обзор

DSP Education Stand — это standalone desktop application на JUCE для изучения базовых DSP-концепций на Windows и macOS.

Приложение показывает простой монофонический audio pipeline с:

- встроенными генераторами сигнала
- встроенными DSP-алгоритмами
- воспроизведением WAV-файлов
- осциллографом
- пресетами для встроенного DSP
- редактированием пользовательского DSP-кода на C++
- реальной компиляцией пользовательского DSP в нативный модуль и hot reload

Это учебный MVP, а не production-ready plugin host.

### Основные возможности

- Источники сигнала: sine, white noise, impulse, WAV file
- Транспорт: start audio, stop audio
- Параметры источника: gain, frequency для sine, информация о sample rate и block size
- Built-in DSP: bypass, gain, hard clip, one-pole lowpass, simple delay
- User DSP mode: редактирование multi-file DSP-проекта, компиляция нативного модуля под текущую платформу, загрузка нового модуля, сохранение предыдущего рабочего модуля при ошибке
- Визуализация: oscilloscope
- Presets: сохранение и загрузка настроек встроенного DSP
- DSP projects: архивы `.dspedu`, дерево файлов и папок, сохранение/загрузка проекта, import/export вспомогательных `.cpp`

### Требования

Общие:

- CMake 3.22 или новее
- Локальный checkout JUCE, переданный через `JUCE_DIR`

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

#### Windows

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

#### macOS

Из корня проекта:

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --target DspEducationStand -j
```

Если удобнее через переменную окружения:

```bash
export JUCE_DIR=/path/to/JUCE
cmake -S . -B build
cmake --build build --target DspEducationStand -j
```

### Запуск

После сборки запусти приложение из каталога build artefacts.

Пример для Windows:

```powershell
& ".\build\DspEducationStand_artefacts\Debug\DSP Education Stand.exe"
```

Пример для macOS:

```bash
open "build/DspEducationStand_artefacts/DSP Education Stand.app"
```

Точный подкаталог может отличаться в зависимости от локального генератора и режима сборки.

### Пользовательский DSP Project Workflow

В приложении реализована настоящая компиляция пользовательского DSP на обеих поддерживаемых desktop-платформах, и основной редактируемой сущностью теперь является проект, а не один текстовый буфер.

Как это работает:

1. `New Project` создаёт дефолтный DSP-проект с папками `src/` и `include/`.
2. Справа Project Navigator показывает дерево файлов и подпапок.
3. Проекты сохраняются и загружаются как ZIP-архивы `.dspedu`.
4. Редактор по-прежнему показывает один файл за раз, но компиляция видит весь проект целиком.
5. Поле Processor Class говорит приложению, какой класс нужно экспортировать из проекта.
6. Во время staging приложение само добавляет `#include "UserDspApi.h"` в translation units.
7. Во время staging приложение само добавляет `DSP_EDU_DEFINE_SIMPLE_PLUGIN(ProcessorClass)`.
8. На Windows вызывается `scripts/build_user_dsp.cmd`, который поднимает MSVC и собирает `.dll`.
9. На macOS вызывается `scripts/build_user_dsp.sh`, который использует `xcrun clang++` и собирает `.dylib`.
10. При успехе приложение загружает новый модуль и безопасно переключает обработку на него.
11. При ошибке предыдущий валидный DSP-модуль остаётся активным.

Пользовательский API остаётся class-based. Пользователь пишет классы процессора, вспомогательные заголовки и дополнительные `.cpp`-файлы. Вся SDK-обвязка остаётся внутренней.

Минимальный пример:

```cpp
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
```

После этого в поле Processor Class нужно указать `MyAudioProcessor` и собрать проект.

Если пользователь вручную добавляет `#include "UserDspApi.h"` или `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`, приложение считает это ошибкой, потому что эти части добавляются автоматически.

Действия в Project Navigator:

- Для root или папки: `New File`, `New Folder`, `Import CPP`
- Для файла или папки: `Rename`, `Move`, `Delete`
- Для файла: `Export CPP`

Поддерживаемые типы исходников в v1: `.cpp`, `.h`, `.hpp`.

### Структура проекта

```text
src/
  app/         Приложение и главное окно
  audio/       Audio engine, генераторы, WAV source
  dsp/         Встроенные DSP processors
  presets/     Менеджер пресетов built-in DSP
  ui/          Компоненты осциллографа
  userdsp/     Редактор кода, компилятор, host модулей
  util/        Вспомогательные утилиты
scripts/       Скрипты сборки для Windows и пользовательских DSP-модулей
templates/     Дефолтный шаблон user DSP
user_dsp_sdk/  Стабильный SDK header для пользовательских DSP-модулей
tests/         Минимальные опциональные тесты
```

### Ограничения и заметки

- Внутренняя обработка в текущем MVP монофоническая.
- Тяжёлая работа, включая file I/O и компиляцию пользовательских модулей, вынесена из audio thread.
- Поддержка macOS в этом репозитории рассчитана на локальные developer build, а не на notarized distribution.
- Versioned naming для output-модулей позволяет не перезаписывать уже загруженный модуль во время hot reload.
