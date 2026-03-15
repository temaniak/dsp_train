# Руководство по DSP Education Stand

Это руководство не про весь язык C++ и не про DSP вообще.

Оно описывает именно текущую работу приложения:

- как писать код под этот хост
- как работать с контролами
- как работает MIDI
- как подключать MIDI-клавиатуру
- как сохраняются проекты

## 1. Что это за программа

`DSP Education Stand` — это standalone-приложение для написания и тестирования пользовательских DSP-модулей.

Программа позволяет:

- создавать и открывать проекты `.dspedu`
- редактировать C++-файлы прямо внутри приложения
- добавлять runtime-контролы в окне `Controls`
- компилировать и hot-reload-ить DSP-модуль
- выбирать аудио- и MIDI-устройства
- быстро проверять эффекты и синты на слух

Это не workflow для упаковки плагинов.
Здесь вы пишете сам DSP-процессор.

## 2. Базовый workflow

Обычный порядок работы такой:

1. Создать новый проект или открыть `.dspedu`.
2. Редактировать исходники в редакторе кода.
3. Указать правильное имя класса в поле `Processor Class`.
4. Открыть `Controls` и добавить нужные ручки, кнопки или тумблеры.
5. Нажать `Compile`.
6. Нажать `Start Audio`.
7. Выбрать источник сигнала или играть по MIDI.

Полезное правило:

- если изменили DSP-код, компилируйте заново
- если изменили состав контролов или их метаданные, тоже компилируйте заново

## 3. Какой вид кода ожидает хост

Хост ожидает процессорный класс примерно такого вида:

```cpp
class MyAudioProcessor
{
public:
    const char* getName() const
    {
        return "My DSP";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredInputChannels = 2;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 1.0 ? spec.sampleRate : 48000.0;
    }

    void reset()
    {
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
    }

private:
    double sampleRate = 48000.0;
};
```

Основные методы:

- `getPreferredAudioConfig(...)`
- `prepare(...)`
- `reset()`
- `process(...)`

## 4. Важные правила именно этого хоста

Эти правила специфичны для данного приложения.

### Не пишите вручную host boilerplate

Не нужно вручную добавлять:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

Приложение само подставляет SDK include, generated controls header, MIDI wrapper и entry wrapper на этапе staging/compile.

### `Processor Class` должен совпадать с кодом

Поле `Processor Class` должно содержать точное имя класса, который нужно экспортировать.

Пример:

- в коде есть `class MyAudioProcessor`
- в поле `Processor Class` должно быть `MyAudioProcessor`

Если класс находится в namespace, указывайте полное имя.

### Preferred config и реальная runtime-конфигурация — не одно и то же

Это важный нюанс:

- `config.preferredSampleRate = 48000.0;` — это только пожелание хосту
- реальные значения приходят в `prepare(const DspEduProcessSpec& spec)`

Поэтому DSP-код всегда должен опираться на:

- `spec.sampleRate`
- `spec.maximumBlockSize`
- реальные количества каналов, пришедшие в `process(...)`

### Код должен быть realtime-safe

Внутри `process(...)` избегайте:

- работы с файлами
- lock/mutex
- выделения памяти в куче
- постоянного resize контейнеров
- блокирующих вызовов

Лучше использовать:

- member-переменные для DSP-состояния
- preallocation в `prepare(...)`
- reset состояния в `reset()`
- простой и предсказуемый код

## 5. Рекомендуемые аудиоконфигурации

Используйте такие шаблоны в зависимости от типа проекта.

### Эффект / обработка входящего сигнала

Обычно:

```cpp
config.preferredInputChannels = 1; // или 2
config.preferredOutputChannels = 2;
```

Подходящие источники в приложении:

- `Hardware Input`
- `WAV`
- `White Noise`
- `Impulse`

### Генерирующий синт без аудиовхода

Обычно:

```cpp
config.preferredInputChannels = 0;
config.preferredOutputChannels = 2;
```

В этом случае DSP сам генерирует звук и не использует `inputs`.

## 6. Источники сигнала в приложении

Приложение может подавать на DSP:

- `Sine`
- `White Noise`
- `Impulse`
- `WAV`
- `Hardware Input`

Используйте их осознанно:

- `Sine` — для проверки gain, clip, modulation
- `White Noise` — для фильтров
- `Impulse` — для delay и reverb
- `WAV` — для общего тестирования
- `Hardware Input` — для live-эффектов

Для синтов внешний source обычно не нужен.

## 7. Работа с Controls

Откройте окно `Controls`, чтобы определить runtime-контролы проекта.

Поддерживаются:

- `Knob`
- `Button`
- `Toggle`

### Как это выглядит в коде

Сгенерированные контролы доступны так:

```cpp
controls.cutoffKnob
controls.triggerButton
controls.bypassToggle
```

Поведение:

- `Knob` даёт float в диапазоне `0..1`
- `Button` ведёт себя как momentary bool
- `Toggle` ведёт себя как latched bool

### Текущая модель редактирования

Сейчас настройка контролов делается прямо внутри карточек control-tiles.

В edit mode можно менять:

- отображаемое имя
- DSP code name
- тип
- MIDI-назначение
- порядок
- удаление

Метаданные сохраняются автоматически внутри текущего состояния проекта в памяти.

Важно:

- сами метаданные карточек сохраняются сразу в текущем проекте
- но в файл `.dspedu` они записываются только через `Save` или `Save As`
- но running DSP-module начинает полноценно работать с текущим layout только после `Compile`

### Preview и linked runtime

Есть два состояния:

- `Linked to DSP`
- `Preview only` или `Compile to relink`

Это означает:

- если текущий layout контролов совпадает с последней собранной версией DSP, контролы live
- если layout менялся после последней компиляции, UI ещё может показывать preview-значения, но для полноценной связки нужен новый `Compile`

### Практические советы по именованию

Используйте понятные code name:

- `cutoffKnob`
- `resonanceKnob`
- `triggerButton`
- `bypassToggle`
- `attackKnob`

Не создавайте свой локальный объект с именем `controls`, иначе вы затените сгенерированный host-object.

## 8. Общая схема MIDI

В приложении есть два разных MIDI-сценария.

Это очень важное различие.

### A. MIDI управляет UI-контролами

Это путь `MIDI -> control value`.

Пример:

- CC 21 двигает `controls.cutoffKnob`
- Note Gate триггерит `controls.triggerButton`
- Pitch Wheel двигает macro knob

Это настраивается внутри `Controls`.

### B. MIDI используется напрямую в DSP-коде

Это путь `MIDI keyboard -> note state -> synth code`.

Пример:

- `midi.noteNumber` задаёт высоту осциллятора
- `midi.gate` запускает и отпускает envelope
- `midi.velocity` задаёт силу ноты
- `midi.voices[]` используется для polyphonic synth

Для этого не нужно сначала маппить MIDI на UI-контрол.

## 9. Выбор MIDI-устройства

В главном окне приложения есть глобальный селектор `MIDI Input`.

Важно:

- выбранное MIDI-устройство является app-wide настройкой
- оно не сохраняется внутри конкретного проекта
- DSP-код сам MIDI-устройства не открывает

Обычный порядок:

1. Выбрать `MIDI Input` в главном окне.
2. Либо маппить MIDI на контролы, либо читать прямое MIDI-состояние через `midi` в DSP-коде.

## 10. MIDI bindings для контролов

Внутри `Controls` каждому контролу можно назначить MIDI.

Доступные источники binding:

- `CC`
- `Note Gate`
- `Note Velocity`
- `Note Number`
- `Pitch Wheel`

Назначение можно сделать:

- вручную через MIDI-меню
- через `Learn`

Снять назначение можно кнопкой `Clear`.

### Ручное назначение

В edit mode внутри карточки:

1. Нажмите на поле `MIDI`.
2. Выберите тип источника.
3. Выберите канал.
4. Выберите CC number или note number, если это нужно.

### Learn

В edit mode внутри карточки:

1. Нажмите `Learn`.
2. Отправьте MIDI-сообщение с контроллера.
3. Назначение захватится и появится в карточке.

### Как нормализуются значения binding

Сейчас правила такие:

- `CC` -> `0..127` становится `0..1`
- `Note Gate` -> `1` на note-on, `0` на note-off
- `Note Velocity` -> velocity note-on становится `0..1`, на note-off сбрасывается в `0`
- `Note Number` -> номер ноты становится `note / 127.0`, на note-off сбрасывается в `0`
- `Pitch Wheel` binding -> `0..1`

Очень важный нюанс:

- `Pitch Wheel`, назначенный на control, работает в диапазоне `0..1`
- прямой `midi.pitchWheel` в DSP-коде работает в диапазоне `-1..1`

## 11. Как писать код под MIDI-клавиатуру

Для настоящего синтового поведения читайте сгенерированный объект `midi` напрямую.

### Моно-состояние клавиатуры

Полезные поля:

```cpp
midi.gate
midi.noteNumber
midi.velocity
midi.pitchWheel
```

Типовая логика моно-синта:

- если `midi.gate` стал активным, запустить ноту
- использовать `midi.noteNumber` для pitch осциллятора
- использовать `midi.velocity` для громкости или тембра
- использовать `midi.pitchWheel` для pitch bend
- если `midi.gate` стал нулём, отпустить envelope

Важно понимать текущее поведение:

- mono convenience state работает по правилу `last note wins`
- если текущая активная моно-нота отпущена, `gate` становится `0`
- автоматического возврата к предыдущей всё ещё удерживаемой ноте нет

Поэтому для более сложной клавиатурной логики используйте `midi.voices[]`.

### Пример моно-использования

```cpp
const bool gateIsHigh = midi.gate != 0 && midi.noteNumber >= 0;

if (gateIsHigh)
{
    const float note = static_cast<float>(midi.noteNumber);
    const float bendSemitones = midi.pitchWheel * 2.0f;
    const float playedNote = note + bendSemitones;
}
```

## 12. Полифония и `midi.voices[]`

Для polyphonic synth используйте:

```cpp
midi.voiceCount
midi.voices[index]
midi.channelPitchWheel[channelIndex]
```

Каждый voice slot содержит:

- `active`
- `channel`
- `noteNumber`
- `velocity`
- `order`

### Как это использовать

Хороший паттерн такой:

1. Держать один synth voice object на каждый host MIDI voice slot.
2. На каждом блоке смотреть `midi.voices[]`.
3. Если slot стал активным с новым `order`, стартовать synth voice.
4. Если slot перестал быть активным, переводить synth voice в release.
5. Позволять release tail продолжаться внутри synth voice даже после того, как MIDI slot стал inactive.

Это правильный подход для ADSR-based poly synth в этом приложении.

### Зачем нужен `order`

Хост может переиспользовать voice slots.

Это значит:

- сейчас slot 3 может соответствовать одной ноте
- позже slot 3 может представлять уже другую ноту

`order` позволяет понять, что в этом slot теперь новое note-event.

## 13. Pitch wheel в прямом DSP MIDI

Прямой keyboard pitch wheel доступен как:

- `midi.pitchWheel` в диапазоне `-1..1`
- `midi.channelPitchWheel[channel - 1]` в диапазоне `-1..1`

Типичное использование:

- mono synth: брать `midi.pitchWheel`
- poly synth: брать `midi.channelPitchWheel[...]` по каналу каждой ноты

Пример:

```cpp
const float bendRangeInSemitones = 2.0f;
const float bentNote = static_cast<float>(midi.noteNumber) + midi.pitchWheel * bendRangeInSemitones;
```

## 14. Как сохраняются проекты

Проекты сохраняются в файлы `.dspedu`.

Фактически `.dspedu` — это zip-архив, внутри которого находятся:

- `project.json`
- `files/...`

### Что хранится в архиве проекта

Внутри проекта сохраняются:

- имя проекта
- имя processor class
- путь к активному файлу
- дерево проекта
- содержимое source/header файлов
- метаданные контролов
- MIDI bindings контролов
- project audio state и routing metadata

### Что в проекте не хранится

Выбранное глобальное MIDI-устройство не хранится внутри `.dspedu`.

Эта настройка живёт отдельно как app-wide setting.

## 15. Save, Save As, Open, New

### Save

- сохраняет в текущий `.dspedu`
- если у проекта ещё нет пути, потребуется `Save As`

### Save As

- позволяет выбрать путь для `.dspedu`
- если расширение не указано, приложение само добавит `.dspedu`

### Open Project

- открывает `.dspedu`
- восстанавливает файлы, контролы, project metadata и project audio state

### New Project

- создаёт новый минимальный проект
- стартует с compile-ready шаблона по умолчанию

Если есть несохранённые изменения, приложение предложит:

- сохранить
- отбросить
- отменить действие

## 16. Рекомендуемые рабочие привычки

Чтобы работать стабильно и предсказуемо:

- выносите вспомогательные DSP-классы в отдельные `.h`, когда проект растёт
- держите `main.cpp` маленьким, если проект стал сложнее
- внимательно следите за полем `Processor Class`
- компилируйте после изменений layout контролов
- осознанно выбирайте source signal
- используйте MIDI bindings для macro и UI-управления
- используйте прямой `midi` state для синтовой клавиатурной логики

## 17. Частые ошибки

### Ошибки host integration

- вручную подключать `UserDspApi.h`
- вручную экспортировать plugin entry
- забывать обновить `Processor Class`
- менять layout контролов и забывать нажать `Compile`

### Ошибки по аудио

- предполагать, что всегда есть stereo
- игнорировать null input/output pointers
- использовать preferred sample rate вместо реального runtime sample rate
- делать слишком много работы внутри `process(...)`

### Ошибки по MIDI

- путать MIDI bindings контролов и прямое keyboard MIDI state
- ожидать, что control-bound pitch wheel будет `-1..1`
- ожидать, что mono `midi.gate` автоматически вернёт предыдущую удерживаемую ноту
- пытаться играть с MIDI-клавиатуры, не выбрав сначала `MIDI Input`

## 18. Хорошие стартовые шаблоны

Полезно держать в голове такие шаблоны:

- pass-through effect: `1-2 input`, `2 output`
- filter effect: входной сигнал + ручки cutoff/resonance
- mono synth: `0 input`, `2 output`, читать `midi.gate` и `midi.noteNumber`
- poly synth: `0 input`, `2 output`, читать `midi.voices[]`

Если придерживаться этих паттернов, приложение будет вести себя предсказуемо и удобно.
