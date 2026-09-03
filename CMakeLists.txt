cmake_minimum_required(VERSION 3.22)

set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
cmake_policy(SET CMP0091 NEW)

project(Foundry VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
set(FOUNDRY_JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout (optional)")

if(FOUNDRY_JUCE_PATH)
    message(STATUS "${PROJECT_NAME}: using local JUCE at ${FOUNDRY_JUCE_PATH}")
    add_subdirectory(${FOUNDRY_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# SQLite (amalgamation, compiled separately so JUCE warning flags don't apply)
# ---------------------------------------------------------------------------
add_library(foundry_sqlite STATIC ThirdParty/sqlite/sqlite3.c)
set_target_properties(foundry_sqlite PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    C_STANDARD 99)
target_include_directories(foundry_sqlite PUBLIC ThirdParty/sqlite)
target_compile_definitions(foundry_sqlite PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_DEPRECATED=1
    SQLITE_OMIT_SHARED_CACHE=1
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_USE_URI=1)
if(MSVC)
    target_compile_options(foundry_sqlite PRIVATE /W0)
else()
    target_compile_options(foundry_sqlite PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# Embedded assets
# ---------------------------------------------------------------------------
juce_add_binary_data(FoundryAssets
    NAMESPACE   templatedata
    HEADER_NAME "TemplateBinaryData.h"
    SOURCES
        Assets/knobs/knob_body_master.png)

set_target_properties(FoundryAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(Foundry
    COMPANY_NAME                "Iwolski Audio"
    COMPANY_WEBSITE             "https://github.com/iwolski99/VST3-Plugin-Template"
    BUNDLE_ID                   "com.iwolskiaudio.foundry"
    PLUGIN_MANUFACTURER_CODE    Iwol
    PLUGIN_CODE                 Fndr
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Foundry"
    DESCRIPTION                 "Loop generation and playback for Riddim and Dubstep"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    NEEDS_CURL                  FALSE
    NEEDS_WEB_BROWSER           FALSE
    VST3_CATEGORIES             Instrument Sampler
    VST3_AUTO_MANIFEST          TRUE)

set(FOUNDRY_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params/ParameterLayout.cpp
    Source/Core/Types.cpp
    Source/Core/ConfigStore.cpp
    Source/Core/LibraryDatabase.cpp
    Source/Core/LibraryScan.cpp
    Source/Core/AudioIO.cpp
    Source/Core/PreviewEngine.cpp
    Source/Core/Generators.cpp
    Source/Core/JobWorker.cpp
    Source/GUI/Theme.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/KnobComponent.cpp
    Source/GUI/HeaderBar.cpp
    Source/GUI/FooterBar.cpp
    Source/GUI/TabBar.cpp
    Source/GUI/BrowserList.cpp
    Source/GUI/SequencerGrid.cpp
    Source/GUI/LibraryPanel.cpp
    Source/GUI/DrumPanel.cpp
    Source/GUI/BassPanel.cpp
    Source/GUI/SplicerPanel.cpp
    Source/GUI/SettingsPanel.cpp
    Source/GUI/ProgressOverlay.cpp)

target_sources(Foundry PRIVATE ${FOUNDRY_SOURCES})
target_include_directories(Foundry PRIVATE Source)

target_compile_definitions(Foundry PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(Foundry PRIVATE
    FoundryAssets
    foundry_sqlite
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(Foundry PRIVATE $<$<CONFIG:Release>:/O2 /Oi /Ot>)
else()
    target_compile_options(Foundry PRIVATE $<$<CONFIG:Release>:-O3>)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(FOUNDRY_BUILD_TESTS "Build engine and lifecycle tests" OFF)
option(TEMPLATE_BUILD_TESTS "Alias for FOUNDRY_BUILD_TESTS" OFF)
option(FOUNDRY_SANITIZE "Build tests with AddressSanitizer + UBSan" OFF)
option(TEMPLATE_SANITIZE "Alias for FOUNDRY_SANITIZE" OFF)

if(TEMPLATE_BUILD_TESTS)
    set(FOUNDRY_BUILD_TESTS ON)
endif()
if(TEMPLATE_SANITIZE)
    set(FOUNDRY_SANITIZE ON)
endif()

if(FOUNDRY_BUILD_TESTS)
    set(FOUNDRY_ENGINE_SOURCES
        Source/Core/Types.cpp
        Source/Core/ConfigStore.cpp
        Source/Core/LibraryDatabase.cpp
        Source/Core/LibraryScan.cpp
        Source/Core/AudioIO.cpp
        Source/Core/PreviewEngine.cpp
        Source/Core/Generators.cpp
        Source/Core/JobWorker.cpp)

    juce_add_console_app(FoundryEngineTest PRODUCT_NAME "FoundryEngineTest")
    target_sources(FoundryEngineTest PRIVATE Tests/EngineTest.cpp ${FOUNDRY_ENGINE_SOURCES})
    target_include_directories(FoundryEngineTest PRIVATE Source)
    target_compile_definitions(FoundryEngineTest PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(FoundryEngineTest PRIVATE
        foundry_sqlite
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_data_structures
        PUBLIC
        juce::juce_recommended_config_flags)

    juce_add_console_app(FoundryPlaybackTest PRODUCT_NAME "FoundryPlaybackTest")
    target_sources(FoundryPlaybackTest PRIVATE Tests/PlaybackTest.cpp ${FOUNDRY_SOURCES})
    target_include_directories(FoundryPlaybackTest PRIVATE Source)
    target_compile_definitions(FoundryPlaybackTest PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FoundryPlaybackTest PRIVATE
        FoundryAssets
        foundry_sqlite
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags)

    juce_add_console_app(FoundryLifecycleTest PRODUCT_NAME "FoundryLifecycleTest")
    target_sources(FoundryLifecycleTest PRIVATE Tests/LifecycleTest.cpp ${FOUNDRY_SOURCES})
    target_include_directories(FoundryLifecycleTest PRIVATE Source)
    target_compile_definitions(FoundryLifecycleTest PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FoundryLifecycleTest PRIVATE
        FoundryAssets
        foundry_sqlite
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags)

    if(FOUNDRY_SANITIZE AND NOT MSVC)
        foreach(t FoundryEngineTest FoundryPlaybackTest FoundryLifecycleTest)
            target_compile_options(${t} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer -g)
            target_link_options(${t} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
