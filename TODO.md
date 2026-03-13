# TODO

Completed items from the previous roadmap were removed from this file.
This list tracks what is still left, with a simple status marker:

- `[partial]` = groundwork already exists
- `[todo]` = not started yet

## 1. Project Onboarding `[partial]`

- `[partial]` A blank minimal project already exists as the default new-project baseline.
- `[todo]` Add a clearer `New Project` onboarding flow instead of an immediate silent reset.
- `[todo]` Let the user choose a project name before the first save.
- `[todo]` Consider letting the user choose between:
  - blank project
  - educational example
- `[todo]` Make example import/open flow obvious from the main UI.

## 2. Example Project UX `[partial]`

- `[partial]` Educational example projects already live in `example_projects/`.
- `[partial]` Ready-to-open `.dspedu` archives now live in `example_project_archives/`.
- `[todo]` Add individual compile smoke tests for the educational examples.
- `[todo]` Consider an in-app "Open Example" action.

## 3. MIDI `[todo]`

- `[todo]` Add MIDI input device selection.
- `[todo]` Add realtime-safe `MIDI -> Control Mapping`.
- `[todo]` Add `MIDI Learn` in the `Controls` window.
- `[todo]` Persist MIDI mappings inside `.dspedu` projects.

## 4. Left Panel Cleanup `[todo]`

- `[todo]` Rebalance the order and density of the left-side controls.
- `[todo]` Reduce visual noise in the audio-device section.
- `[todo]` Re-check scrolling and accessibility on smaller window heights.

## 5. Validation And Polish `[partial]`

- `[partial]` Basic automated coverage for project archive save/load already exists.
- `[todo]` Add more automated coverage around project archive loading/saving.
- `[todo]` Consider compile-time validation helpers for common user-DSP mistakes.
- `[todo]` Address the remaining `parentComponent` shadowing warning in `src/ui/ToolWindow.cpp`.
