cmake_minimum_required(VERSION 3.22)

project(Otoha VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SQLite -----------------------------------------------------------------
# Prefer the system dev package (libsqlite3-dev on Debian/Ubuntu). On Windows
# there is no system SQLite, so fall back to fetching the official
# amalgamation from sqlite.org — keeping "clone -> configure -> build" a
# zero-extra-steps experience on every platform.
find_package(SQLite3 QUIET)

if (NOT SQLite3_FOUND)
    message(STATUS "SQLite3 not found locally - fetching the official amalgamation")
    include(FetchContent)
    FetchContent_Declare(
        sqlite_amalgamation
        URL      https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
        URL_HASH SHA256=5592243caf28b2cdef41e6ab58d25d653dfc53deded8450eb66072c929f030c4)
    FetchContent_MakeAvailable(sqlite_amalgamation)

    add_library(sqlite3_local STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_local PUBLIC ${sqlite_amalgamation_SOURCE_DIR})

    # Expose it through the same variables the targets below already consume.
    set(SQLite3_INCLUDE_DIRS ${sqlite_amalgamation_SOURCE_DIR})
    set(SQLite3_LIBRARIES sqlite3_local)
endif()

# --- JUCE (downloaded automatically on first configure) ----------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# --- Otoha application --------------------------------------------------------
juce_add_gui_app(Otoha
    PRODUCT_NAME    "Otoha"
    COMPANY_NAME    "Otoha"
    BUNDLE_ID       "app.otoha.desktop"
    NEEDS_MENUBAR   FALSE)

# JUCE 8 disables modal loops by default; the export dialog's synchronous
# flow relies on one, so opt back in explicitly below (desktop app only).

target_sources(Otoha PRIVATE
    Source/App/Main.cpp
    Source/App/MainWindow.cpp
    Source/Audio/Player.cpp
    Source/Audio/Recorder.cpp
    Source/Core/RecordingSupport.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp
    Source/Dsp/SampleRateConverter.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/ProjectFormat.cpp
    Source/Editor/TimelineRenderer.cpp
    Source/Editor/TimelineSource.cpp
    Source/Export/AudioExporter.cpp
    Source/Export/ExportManager.cpp
    Source/Export/ExportPresets.cpp
    Source/Export/FfmpegSupport.cpp
    Source/Export/Naming.cpp
    Source/Library/Database.cpp
    Source/Library/LibraryService.cpp
    Source/Library/WaveformCache.cpp
    Source/UI/AppShell.cpp
    Source/UI/EditorView.cpp
    Source/UI/EnhancePanel.cpp
    Source/UI/ExportUi.cpp
    Source/UI/HomeView.cpp
    Source/UI/LibraryView.cpp
    Source/UI/OnboardingView.cpp
    Source/UI/RecordView.cpp
    Source/UI/SoundView.cpp
    Source/UI/SettingsView.cpp)

# M12: the WASAPI Sound backend is Windows-only; other platforms build the app
# (Studio + honest unsupported-Sound placeholder) without it.
if(WIN32)
    target_sources(Otoha PRIVATE Source/Sound/platform/WindowsAudioBackend.cpp)
endif()

target_include_directories(Otoha PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(Otoha PRIVATE ${SQLite3_LIBRARIES})

# OTOHA_PLATFORM_* feeds PlatformCapabilities::current() (#50).
if(WIN32)
    set(OTOHA_PLATFORM_DEFINE OTOHA_PLATFORM_WINDOWS=1)
elseif(APPLE)
    set(OTOHA_PLATFORM_DEFINE OTOHA_PLATFORM_MACOS=1)
elseif(ANDROID)
    set(OTOHA_PLATFORM_DEFINE OTOHA_PLATFORM_ANDROID=1)
else()
    set(OTOHA_PLATFORM_DEFINE OTOHA_PLATFORM_LINUX=1)
endif()

# M16 #4: build identity baked into About/diagnostics. Release tooling
# (scripts/release.sh / CI) overrides the date for reproducibility.
find_package(Git QUIET)
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE OTOHA_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT OTOHA_GIT_COMMIT)
    set(OTOHA_GIT_COMMIT "dev")
endif()
string(TIMESTAMP OTOHA_BUILD_DATE "%Y-%m-%d")

target_compile_definitions(Otoha PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_MODAL_LOOPS_PERMITTED=1   # export dialog uses one synchronous modal loop
    OTOHA_VERSION="${PROJECT_VERSION}"
    OTOHA_GIT_COMMIT="${OTOHA_GIT_COMMIT}"
    OTOHA_BUILD_DATE="${OTOHA_BUILD_DATE}"
    ${OTOHA_PLATFORM_DEFINE})

target_link_libraries(Otoha PRIVATE
    juce::juce_gui_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_data_structures)   # PropertiesFile (ExportPresets)

juce_generate_juce_header(Otoha)

# --- Headless core tests (no audio device needed, CI friendly) ----------------
# One small binary per suite; every suite is device-independent by design.
juce_add_console_app(otoha_library_tests PRODUCT_NAME "otoha_library_tests")
target_sources(otoha_library_tests PRIVATE
    Tests/LibraryTests.cpp
    Source/Library/Database.cpp
    Source/Library/LibraryService.cpp
    Source/Library/WaveformCache.cpp
    Source/Core/RecordingSupport.cpp)

target_include_directories(otoha_library_tests PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(otoha_library_tests PRIVATE
    juce::juce_audio_formats
    ${SQLite3_LIBRARIES})

juce_add_console_app(otoha_tests PRODUCT_NAME "otoha_tests")
target_sources(otoha_tests PRIVATE Tests/WavRoundTripTest.cpp)

juce_add_console_app(otoha_edit_tests PRODUCT_NAME "otoha_edit_tests")
target_sources(otoha_edit_tests PRIVATE
    Tests/EditEngineTests.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp
    Source/Dsp/SampleRateConverter.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/TimelineRenderer.cpp)

target_link_libraries(otoha_edit_tests PRIVATE
    juce::juce_audio_formats)

juce_add_console_app(otoha_dsp_tests PRODUCT_NAME "otoha_dsp_tests")
target_sources(otoha_dsp_tests PRIVATE
    Tests/DspEngineTests.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp)

target_link_libraries(otoha_dsp_tests PRIVATE
    juce::juce_audio_basics)

# --- Milestone 7: DSP Core + Otoha Sound platform abstractions ----------------
# Deliberately minimal linkage: proves the DSP Core needs no UI / Library /
# Editor / FFmpeg / platform-audio dependencies to run.
juce_add_console_app(otoha_dspcore_tests PRODUCT_NAME "otoha_dspcore_tests")
target_sources(otoha_dspcore_tests PRIVATE
    Tests/DspCoreTests.cpp
    Source/Dsp/Core/Processors.cpp)

target_link_libraries(otoha_dspcore_tests PRIVATE juce::juce_audio_basics)

# --- Milestone 8: Otoha Sound engine (mock-backend, no hardware) ---------------
juce_add_console_app(otoha_sound_tests PRODUCT_NAME "otoha_sound_tests")
target_sources(otoha_sound_tests PRIVATE
    Tests/SoundEngineTests.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp)

target_link_libraries(otoha_sound_tests PRIVATE juce::juce_audio_basics)

juce_add_console_app(otoha_export_tests PRODUCT_NAME "otoha_export_tests")
target_sources(otoha_export_tests PRIVATE
    Tests/ExportTests.cpp
    Source/Core/RecordingSupport.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp
    Source/Dsp/SampleRateConverter.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/TimelineRenderer.cpp
    Source/Export/AudioExporter.cpp
    Source/Export/ExportManager.cpp
    Source/Export/ExportPresets.cpp
    Source/Export/FfmpegSupport.cpp
    Source/Export/Naming.cpp)

target_include_directories(otoha_export_tests PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(otoha_export_tests PRIVATE
    juce::juce_audio_formats
    juce::juce_data_structures   # PropertiesFile (ExportPresets)
    ${SQLite3_LIBRARIES})

juce_add_console_app(otoha_state_tests PRODUCT_NAME "otoha_state_tests")
target_sources(otoha_state_tests PRIVATE Tests/StateMachineTest.cpp)

juce_add_console_app(otoha_support_tests PRODUCT_NAME "otoha_support_tests")
target_sources(otoha_support_tests PRIVATE
    Tests/SupportTest.cpp
    Source/Core/RecordingSupport.cpp)

# --- Milestone 10: release hardening (settings/presets/lifecycle/artifacts) ----
juce_add_console_app(otoha_release_tests PRODUCT_NAME "otoha_release_tests")
target_sources(otoha_release_tests PRIVATE
    Tests/ReleaseTests.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp)

target_compile_definitions(otoha_release_tests PRIVATE OTOHA_VERSION="${PROJECT_VERSION}")
target_link_libraries(otoha_release_tests PRIVATE juce::juce_audio_basics)

# --- Milestone 12: cross-platform shared core ---------------------------------
# Proves the shared modules (recorder phases, capabilities, error model, .otoha
# project format) are headless and platform-independent — no Windows code, no UI.
juce_add_console_app(otoha_xplat_tests PRODUCT_NAME "otoha_xplat_tests")
target_sources(otoha_xplat_tests PRIVATE
    Tests/CrossPlatformTests.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/ProjectFormat.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp
    Source/Dsp/SampleRateConverter.cpp)

target_link_libraries(otoha_xplat_tests PRIVATE juce::juce_audio_formats)

# --- Milestone 15: QA stress / audio correctness ---------------------------------
# Deliberately abusive: buffer/rate matrices, signal correctness, parameter
# extremes, reset isolation, property-based timeline fuzzing, undo/redo stress,
# unicode + damaged-project handling.
juce_add_console_app(otoha_qa_tests PRODUCT_NAME "otoha_qa_tests")
target_sources(otoha_qa_tests PRIVATE
    Tests/QaStressTests.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/ProjectFormat.cpp
    Source/Dsp/Core/Processors.cpp
    Source/Dsp/DspChain.cpp
    Source/Dsp/Presets.cpp
    Source/Dsp/SampleRateConverter.cpp)

target_link_libraries(otoha_qa_tests PRIVATE juce::juce_audio_formats)

# --- Milestone 18/20: design-system component kit + Studio home ----------------
juce_add_console_app(otoha_ds_tests PRODUCT_NAME "otoha_ds_tests")
target_sources(otoha_ds_tests PRIVATE
    Tests/DsComponentsTests.cpp
    Source/UI/HomeView.cpp
    Source/Library/Database.cpp
    Source/Library/LibraryService.cpp
    Source/Library/WaveformCache.cpp
    Source/Core/RecordingSupport.cpp)
target_include_directories(otoha_ds_tests PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(otoha_ds_tests PRIVATE juce::juce_gui_basics ${SQLite3_LIBRARIES})
target_compile_definitions(otoha_ds_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

# All console test targets: disable web-browser/curl so headless Linux CI
# builds link without libcurl (JUCE's curl path is unused by Otoha).
foreach(OTOHA_TEST_TARGET
        otoha_tests
        otoha_state_tests
        otoha_support_tests
        otoha_library_tests
        otoha_edit_tests
        otoha_dsp_tests
        otoha_dspcore_tests
        otoha_sound_tests
        otoha_export_tests
        otoha_release_tests
        otoha_qa_tests
        otoha_xplat_tests
        otoha_ds_tests)
    target_compile_definitions(${OTOHA_TEST_TARGET} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(${OTOHA_TEST_TARGET} PRIVATE
        juce::juce_audio_formats)
endforeach()

enable_testing()
add_test(NAME wav_round_trip COMMAND otoha_tests)
add_test(NAME state_machine COMMAND otoha_state_tests)
add_test(NAME support COMMAND otoha_support_tests)
add_test(NAME library COMMAND otoha_library_tests)
add_test(NAME edit_engine COMMAND otoha_edit_tests)
add_test(NAME dsp_engine COMMAND otoha_dsp_tests)
add_test(NAME dsp_core COMMAND otoha_dspcore_tests)
add_test(NAME sound_engine COMMAND otoha_sound_tests)
add_test(NAME export_system COMMAND otoha_export_tests)
add_test(NAME release_hardening COMMAND otoha_release_tests)
add_test(NAME cross_platform COMMAND otoha_xplat_tests)
add_test(NAME qa_stress COMMAND otoha_qa_tests)
add_test(NAME ds_components COMMAND otoha_ds_tests)
