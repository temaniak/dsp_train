# HANDOFF

## 1. Goal

Current milestone goal:

- Keep one shared JUCE desktop codebase for Windows and macOS.
- Turn the user DSP area from a single-file editor into a project-based workflow.
- Support `.dspedu` project save/load, multi-file compilation, and right-side project navigator.
- Keep runtime user DSP compilation/hot reload working on macOS and Windows.
- Move the editor area toward an IDE-like UX with:
  - dark professional theme,
  - movable splitters,
  - code font size control.

## 2. What Is Already Done

Cross-platform and user DSP pipeline:

- `JUCE_DIR` is platform-neutral in CMake.
- macOS build support was added alongside Windows.
- user DSP build scripts now support platform-specific shared modules:
  - Windows: `.dll`
  - macOS: `.dylib`
- user DSP compilation no longer works from one raw source string; it now stages a whole project snapshot and builds all `.cpp` files.

Project model:

- Added `UserDspProjectManager` with:
  - in-memory project tree,
  - one active `CodeDocument`,
  - project metadata,
  - dirty tracking,
  - ZIP archive save/load as `.dspedu`,
  - create/rename/move/delete/import/export actions.
- Added `UserDspProjectUtils` for:
  - validation,
  - legacy wrapper stripping,
  - processor class detection helpers,
  - wrapper generation helpers.
- Default new project now creates:
  - `src/`
  - `include/`
  - `src/ExampleUserProcessor.cpp`

UI:

- Main UI is now project-first.
- Added right-side project navigator tree.
- Added toolbar actions:
  - `New Project`
  - `Open Project`
  - `Save Project`
  - `Save Project As`
  - `Compile`
- Added dirty project prompt flow on:
  - new project,
  - open project,
  - app close / quit.
- Added movable splitters:
  - editor vs navigator
  - workspace vs compile log
- Added code font size control in the editor toolbar.
- Applied dark IDE-style theme across the editor-facing UI:
  - main background,
  - panels,
  - toolbar,
  - tree,
  - code editor,
  - compile log,
  - popups and menus,
  - status areas.

Templates and workflow:

- user DSP class-only workflow is in place:
  - users do not manually include `UserDspApi.h`
  - users do not manually write `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`
- wrapper injection stays internal.
- legacy `.cpp` import strips manual SDK include/export macro.

Testing support:

- Added regression coverage in `tests/TestMain.cpp` for:
  - project archive save/load roundtrip,
  - nested folders/files,
  - legacy import stripping,
  - processor class persistence.

## 3. Changed Files

Modified:

- `.gitignore`
- `CMakeLists.txt`
- `README.md`
- `scripts/build_user_dsp.cmd`
- `src/app/DspEducationStandApplication.cpp`
- `src/app/MainComponent.cpp`
- `src/app/MainComponent.h`
- `src/app/MainWindow.cpp`
- `src/app/MainWindow.h`
- `src/ui/OscilloscopeComponent.cpp`
- `src/userdsp/UserCodeDocumentManager.cpp`
- `src/userdsp/UserCodeDocumentManager.h`
- `src/userdsp/UserDspCompiler.cpp`
- `src/userdsp/UserDspCompiler.h`
- `src/userdsp/UserDspHost.cpp`
- `templates/UserDspTemplate.cpp`
- `tests/TestMain.cpp`

New:

- `scripts/build_user_dsp.sh`
- `src/userdsp/UserDspProjectManager.cpp`
- `src/userdsp/UserDspProjectManager.h`
- `src/userdsp/UserDspProjectUtils.cpp`
- `src/userdsp/UserDspProjectUtils.h`
- `HANDOFF.md`

## 4. What Still Needs To Be Done

High-value next steps:

- Manually smoke-test the GUI after the latest dark-theme/splitter/font-size changes:
  - drag both splitters,
  - change code font size,
  - create/move/rename/delete files and folders,
  - open/save `.dspedu`,
  - compile from a multi-file project,
  - check popup menus and dialogs visually.
- Decide whether to fully remove old `UserCodeDocumentManager` sources from the repository, not just from the main app target.
- Update README further if needed to mention:
  - dark IDE theme,
  - movable splitters,
  - code font size control.
- Consider persisting UI/editor preferences:
  - code font size,
  - splitter positions,
  - tree openness state across launches.
- Clean up remaining compiler warnings in:
  - `src/audio/WavFileSource.cpp`
  - `src/presets/PresetManager.cpp`

## 5. Commands / Tests Run And Results

Builds:

- `cmake -S . -B build -DJUCE_DIR=/Users/evgeniystrukov/Desktop/JuceProject/JUCE`
  - result: succeeded
- `cmake -S . -B build -DDSP_EDU_BUILD_TESTS=ON -DJUCE_DIR=/Users/evgeniystrukov/Desktop/JuceProject/JUCE`
  - result: succeeded
- `cmake --build build --target DspEducationStand -j 4`
  - result: succeeded
- `cmake --build build --target DspEducationStandTests -j 4`
  - result: succeeded

Tests:

- `./build/DspEducationStandTests_artefacts/DspEducationStandTests`
  - result: exit code `0`

Manual pipeline smoke check:

- `/bin/sh scripts/build_user_dsp.sh <temp_project_dir> /Users/evgeniystrukov/dsp_train/user_dsp_sdk <temp_out.dylib> <temp_out.dSYM>`
  - result: succeeded, `.dylib` produced from a multi-file temp project

macOS app artifact:

- `build/DspEducationStand_artefacts/DSP Education Stand.app`
  - result: app target builds successfully

## 6. Known Problems And Hypotheses

Known issues:

- The native macOS title bar is still native because the app uses `setUsingNativeTitleBar(true)`. The custom dark theme does not recolour that title bar.
- The latest UI work is compile-verified but not manually smoke-tested after the final theme/splitter/font-size pass.
- There are still old warnings in:
  - `src/audio/WavFileSource.cpp:15`
  - `src/presets/PresetManager.cpp:47`

Hypotheses / likely causes if something looks wrong in the new UI:

- If popup/dialog colours look partially inconsistent, some JUCE native dialog elements may still be controlled by platform look-and-feel rather than custom component colours.
- If splitter positions feel odd on first launch, the current layout uses reasonable defaults but does not yet persist user-adjusted sizes.
- If code syntax colours are not ideal for all token kinds, JUCE C++ tokeniser exposes only a limited token set (`Error`, `Comment`, `Keyword`, `Operator`, `Identifier`, `Integer`, `Float`, `String`, `Bracket`, `Punctuation`, `Preprocessor Text`), so the requested semantic palette is approximated rather than fully semantic.
- `UserCodeDocumentManager` still exists in the repo as legacy code even though the main app now works through `UserDspProjectManager`; that can confuse future maintenance if left in place.
