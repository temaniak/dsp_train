cmake_minimum_required(VERSION 3.22)

project(DspEducationStand VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Path to the local JUCE checkout")
option(DSP_EDU_BUILD_TESTS "Build minimal core logic tests" OFF)

if (JUCE_DIR STREQUAL "")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE checkout. Pass -DJUCE_DIR=/path/to/JUCE or set the JUCE_DIR environment variable.")
endif()

if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE checkout. Current value: ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_gui_app(DspEducationStand
    COMPANY_NAME "OpenAI"
    PRODUCT_NAME "DSP Education Stand"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icon.png")

juce_generate_juce_header(DspEducationStand)

target_sources(DspEducationStand
    PRIVATE
        src/main.cpp
        src/app/DspEducationStandApplication.cpp
        src/app/MainWindow.cpp
        src/app/MainComponent.cpp
        src/audio/AudioEngine.cpp
        src/midi/MidiBinding.cpp
        src/midi/MidiPerformanceState.cpp
        src/audio/SignalSources.cpp
        src/audio/WavFileSource.cpp
        src/ui/DarkIdeLookAndFeel.cpp
        src/ui/OscilloscopeBuffer.cpp
        src/ui/OscilloscopeComponent.cpp
        src/ui/RealtimeControlsComponent.cpp
        src/ui/ToolWindow.cpp
        src/util/AppSettings.cpp
        src/userdsp/UserDspCompiler.cpp
        src/userdsp/UserDspProjectManager.cpp
        src/userdsp/UserDspProjectUtils.cpp
        src/userdsp/UserDspHost.cpp)

target_include_directories(DspEducationStand
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/user_dsp_sdk")

target_compile_definitions(DspEducationStand
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DspEducationStand,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DspEducationStand,JUCE_VERSION>")

target_link_libraries(DspEducationStand
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_target_properties(DspEducationStand PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

if (MSVC)
    target_compile_options(DspEducationStand PRIVATE /utf-8)
endif()

if (DSP_EDU_BUILD_TESTS)
    juce_add_console_app(DspEducationStandTests
        PRODUCT_NAME "DspEducationStandTests")

    juce_generate_juce_header(DspEducationStandTests)

    target_sources(DspEducationStandTests
        PRIVATE
            tests/TestMain.cpp
            tests/ExampleMonoSynthCompileSmoke.cpp
            tests/ExamplePolySynthCompileSmoke.cpp
            src/midi/MidiBinding.cpp
            src/midi/MidiPerformanceState.cpp
            src/userdsp/UserDspProjectManager.cpp
            src/userdsp/UserDspProjectUtils.cpp)

    target_include_directories(DspEducationStandTests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}/user_dsp_sdk")

    target_compile_definitions(DspEducationStandTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(DspEducationStandTests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_core
            juce::juce_data_structures
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    if (MSVC)
        target_compile_options(DspEducationStandTests PRIVATE /utf-8)
    endif()
endif()
