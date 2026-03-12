# TODO

## 1. Убрать встроенные DSP

- Удалить `Built-in DSP` режим из UI и оставить основной фокус на user DSP проектах.
- Убрать selector built-in processor, preset flow и связанные элементы левой панели.
- Удалить или изолировать код в `src/dsp/`, если он больше не нужен в runtime.
- Упростить `AudioEngine`, убрав ветку built-in processing.

## 2. Добавить MIDI

- Добавить выбор MIDI input device в UI.
- Реализовать слой `MIDI -> Control Mapping`.
- Добавить `MIDI Learn` для контроллеров в окне `Controls`.
- Поддержать:
  - `CC -> Knob`
  - `Note/CC -> Button`
  - `Note/CC -> Toggle`
- Сохранять MIDI mappings в `.dspedu` проекте.
- Продумать realtime-safe передачу MIDI событий в runtime controls.

## 3. Откорректировать боковое меню

- Пересобрать левую панель в более компактный и понятный layout.
- Сократить визуальный шум и выделить основные действия.
- Проверить поведение scroll, collapse/expand и доступность кнопок на маленькой высоте окна.
- Уточнить порядок секций после удаления built-in DSP.

## 4. Сделать init project

- Добавить более продуманный сценарий создания нового проекта.
- На старте проекта задавать:
  - имя проекта
  - стартовый template
  - стартовые controls
  - базовую preferred audio configuration
- Свести `New Project` к понятному onboarding flow, а не просто к созданию файлов по умолчанию.

## 5. Сделать больше обучающих проектов

- Подготовить несколько example projects в стиле учебных лабораторий.
- Минимальный набор:
  - oscillator
  - tremolo
  - filter
  - delay
  - distortion
  - envelope / gate
  - stereo routing example
  - MIDI control example
- Для каждого примера показать:
  - структуру проекта
  - controls
  - связь `controls.<codeName>` с кодом
  - preferred audio configuration
