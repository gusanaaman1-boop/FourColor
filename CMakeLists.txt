cmake_minimum_required(VERSION 3.22)

# Universal macOS binary: some hosts on this machine run under Rosetta, and the
# family convention (DigiMeter, TRIX, DualSpace) is to build both slices.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(FourColor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives outside this tree - the same ~/JUCE checkout (JUCE 9) every plugin
# in this workspace uses. CI would check JUCE out into ./JUCE instead.
if(DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
    set(FOURCOLOR_JUCE_DIR "$ENV{HOME}/JUCE")
elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/JUCE/CMakeLists.txt")
    set(FOURCOLOR_JUCE_DIR "$ENV{USERPROFILE}/JUCE")
else()
    set(FOURCOLOR_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()
add_subdirectory(${FOURCOLOR_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

if(APPLE)
    set(FOURCOLOR_FORMATS VST3 AU Standalone)
else()
    set(FOURCOLOR_FORMATS VST3 Standalone)
endif()

option(FOURCOLOR_COPY_AFTER_BUILD "Copy built plug-ins into the user plug-in folders" ON)

juce_add_plugin(FourColor
    COMPANY_NAME "Naaman"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${FOURCOLOR_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Naam
    PLUGIN_CODE Fclr
    FORMATS ${FOURCOLOR_FORMATS}
    PRODUCT_NAME "FourColor")

# Everything except the plugin entry points, shared by the plugin and the tools.
# Files are added here phase by phase as they come into existence.
set(FOURCOLOR_CORE_SOURCES
    Source/Core/PresetLibrary.cpp
    Source/Dsp/AutoLevel.cpp
    Source/Dsp/BandProcessor.cpp
    Source/Dsp/BehaviorDetector.cpp
    Source/Dsp/ColorEngine.cpp
    Source/Dsp/Crossover.cpp
    Source/Dsp/FourColorEngine.cpp
    Source/Dsp/HarmonicSpace.cpp
    Source/Dsp/NonlinearStage.cpp
    Source/Dsp/ToneStage.cpp
    Source/Ui/BandCards.cpp
    Source/Ui/BandStrip.cpp
    Source/Ui/CrossoverDisplay.cpp
    Source/Ui/GlobalBar.cpp
    Source/Ui/Knob.cpp
    Source/Ui/Theme.cpp
    Source/Ui/TopBar.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

target_sources(FourColor PRIVATE ${FOURCOLOR_CORE_SOURCES})

target_compile_definitions(FourColor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FourColor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- DSP + state test suite ----------------------------------------------------
#     build/FourColorTests_artefacts/<config>/FourColorTests
juce_add_console_app(FourColorTests PRODUCT_NAME "FourColorTests")
juce_generate_juce_header(FourColorTests)
target_sources(FourColorTests PRIVATE Source/Tools/test_dsp.cpp ${FOURCOLOR_CORE_SOURCES})
target_compile_definitions(FourColorTests PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(FourColorTests
    PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags)

# --- deterministic screenshot tool (added in Phase 8) --------------------------
#     build/FourColorShot_artefacts/<config>/FourColorShot ui-shots
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/screenshot_tool.cpp")
    juce_add_console_app(FourColorShot PRODUCT_NAME "FourColorShot")
    juce_generate_juce_header(FourColorShot)
    target_sources(FourColorShot PRIVATE Source/Tools/screenshot_tool.cpp ${FOURCOLOR_CORE_SOURCES})
    target_compile_definitions(FourColorShot PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(FourColorShot
        PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags)
endif()
