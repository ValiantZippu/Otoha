cmake_minimum_required(VERSION 3.22)

project(Otoha VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SQLite (system dev package; e.g. libsqlite3-dev on Debian/Ubuntu) -------
find_package(SQLite3 REQUIRED)

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

target_sources(Otoha PRIVATE
    Source/App/Main.cpp
    Source/App/MainWindow.cpp
    Source/Audio/Player.cpp
    Source/Audio/Recorder.cpp
    Source/Core/RecordingSupport.cpp
    Source/Editor/AudioDocument.cpp
    Source/Editor/TimelineRenderer.cpp
    Source/Editor/TimelineSource.cpp
    Source/Library/Database.cpp
    Source/Library/LibraryService.cpp
    Source/Library/WaveformCache.cpp
    Source/UI/AppShell.cpp
    Source/UI/LibraryView.cpp
    Source/UI/RecordView.cpp)

target_include_directories(Otoha PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(Otoha PRIVATE ${SQLite3_LIBRARIES})

target_compile_definitions(Otoha PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    OTOHA_VERSION="${PROJECT_VERSION}")

target_link_libraries(Otoha PRIVATE
    juce::juce_gui_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils)

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
    Source/Editor/AudioDocument.cpp
    Source/Editor/TimelineRenderer.cpp)

target_link_libraries(otoha_edit_tests PRIVATE
    juce::juce_audio_formats)

juce_add_console_app(otoha_state_tests PRODUCT_NAME "otoha_state_tests")
target_sources(otoha_state_tests PRIVATE Tests/StateMachineTest.cpp)

juce_add_console_app(otoha_support_tests PRODUCT_NAME "otoha_support_tests")
target_sources(otoha_support_tests PRIVATE Tests/SupportTest.cpp)

foreach(OTOHA_TEST_TARGET otoha_tests otoha_state_tests otoha_support_tests)
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
