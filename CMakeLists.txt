cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime so the shipped VST3 has no VC++ redistributable dependency.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
cmake_policy(SET CMP0091 NEW)

# ---------------------------------------------------------------------------
# TEMPLATE: rename me. project(<Name> ...) and the juce_add_plugin() target
# name below should both become your plugin's name - see
# scripts/rename_plugin.py, which does this (and everything else on this
# page) for you.
# ---------------------------------------------------------------------------
project(PluginTemplate VERSION 0.1.0 LANGUAGES C CXX)

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
# By default JUCE is downloaded automatically. If you already have a local
# copy, configure with -DTEMPLATE_JUCE_PATH=/path/to/JUCE to use it instead.
set(TEMPLATE_JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout (optional)")

if(TEMPLATE_JUCE_PATH)
    message(STATUS "${PROJECT_NAME}: using local JUCE at ${TEMPLATE_JUCE_PATH}")
    add_subdirectory(${TEMPLATE_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Embedded assets
# ---------------------------------------------------------------------------
# The knob body is a rendered photo, not vector art: JUCE rotates the cached
# Image with an AffineTransform at draw time, so this one PNG covers every
# angle rather than needing a pre-rendered filmstrip. Replace the file itself
# to reskin the knob; the code that draws it does not need to change.
juce_add_binary_data(TemplateAssets
    NAMESPACE   templatedata
    HEADER_NAME "TemplateBinaryData.h"
    SOURCES
        Assets/knobs/knob_body_master.png)

set_target_properties(TemplateAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
# TEMPLATE: every value below (except FORMATS/IS_SYNTH/etc, which describe
# what kind of plugin this is) is placeholder identity that scripts/rename_plugin.py
# fills in for you. If you're doing it by hand: COMPANY_NAME, COMPANY_WEBSITE,
# BUNDLE_ID, PLUGIN_MANUFACTURER_CODE (4 chars, first must be uppercase),
# PLUGIN_CODE (4 chars, must be unique per-plugin within your manufacturer),
# PRODUCT_NAME and DESCRIPTION.
juce_add_plugin(PluginTemplate
    COMPANY_NAME                "Your Company"
    COMPANY_WEBSITE             "https://example.com"
    BUNDLE_ID                   "com.yourcompany.plugintemplate"
    PLUGIN_MANUFACTURER_CODE    Ycmp
    PLUGIN_CODE                 Tmpl
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "PluginTemplate"
    DESCRIPTION                 "A JUCE VST3 plugin"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE   # needed to type preset names
    COPY_PLUGIN_AFTER_BUILD     TRUE
    NEEDS_CURL                  FALSE
    NEEDS_WEB_BROWSER           FALSE
    VST3_CATEGORIES             Fx
    VST3_AUTO_MANIFEST          TRUE)

set(TEMPLATE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params/ParameterLayout.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/GUI/Theme.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/KnobComponent.cpp
    Source/GUI/HeaderBar.cpp
    Source/GUI/FooterBar.cpp
    Source/GUI/MainPanel.cpp
    Source/GUI/SavePresetPanel.cpp
    Source/GUI/PresetBrowser.cpp)

target_sources(PluginTemplate PRIVATE ${TEMPLATE_SOURCES})

target_include_directories(PluginTemplate PRIVATE Source)

target_compile_definitions(PluginTemplate PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0)

target_link_libraries(PluginTemplate PRIVATE
    TemplateAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(PluginTemplate PRIVATE $<$<CONFIG:Release>:/O2 /Oi /Ot>)
else()
    target_compile_options(PluginTemplate PRIVATE $<$<CONFIG:Release>:-O3>)
endif()

# ---------------------------------------------------------------------------
# Tests (opt in: -DTEMPLATE_BUILD_TESTS=ON)
# ---------------------------------------------------------------------------
option(TEMPLATE_BUILD_TESTS "Build the offline DSP smoke test and lifecycle test" OFF)
option(TEMPLATE_SANITIZE "Build the tests with AddressSanitizer + UBSan" OFF)

if(TEMPLATE_BUILD_TESTS)
    juce_add_console_app(TemplateDspTest PRODUCT_NAME "TemplateDspTest")
    target_sources(TemplateDspTest PRIVATE Tests/DspSmokeTest.cpp ${TEMPLATE_SOURCES})
    target_include_directories(TemplateDspTest PRIVATE Source)
    target_compile_definitions(TemplateDspTest PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0)
    target_link_libraries(TemplateDspTest PRIVATE
        TemplateAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        PUBLIC
        juce::juce_recommended_config_flags)

    # Construct/run/destroy the processor and editor the way a host does, so
    # that teardown bugs surface here rather than as "the DAW hangs when I
    # delete the plugin". Worth running under -DTEMPLATE_SANITIZE=ON, which is
    # what turns a latent use-after-free into a deterministic, located report.
    juce_add_console_app(TemplateLifecycleTest PRODUCT_NAME "TemplateLifecycleTest")
    target_sources(TemplateLifecycleTest PRIVATE Tests/LifecycleTest.cpp ${TEMPLATE_SOURCES})
    target_include_directories(TemplateLifecycleTest PRIVATE Source)
    target_compile_definitions(TemplateLifecycleTest PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(TemplateLifecycleTest PRIVATE
        TemplateAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        PUBLIC
        juce::juce_recommended_config_flags)

    if(TEMPLATE_SANITIZE AND NOT MSVC)
        foreach(t TemplateDspTest TemplateLifecycleTest)
            target_compile_options(${t} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer -g)
            target_link_options(${t} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
