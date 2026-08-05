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
option(FOURCOLOR_WERROR "Treat warnings in FOUR COLOR's own sources as errors" OFF)
option(FOURCOLOR_SANITIZERS "Add a FourColorTestsSan target built with ASan/UBSan" OFF)

# Warning policy. JUCE's module sources are compiled into our targets, so a
# blanket -Werror on the target would fail on third-party code we do not own.
# These flags are attached to FOUR COLOR's own translation units only, which is
# what "0 warnings from FOUR COLOR code" can honestly mean.
if(MSVC)
    set(FOURCOLOR_WARNING_FLAGS /W4 /permissive-)
    if(FOURCOLOR_WERROR)
        list(APPEND FOURCOLOR_WARNING_FLAGS /WX)
    endif()
else()
    # Deliberately NOT enabled: -Wconversion, -Wsign-conversion,
    # -Wdouble-promotion and -Wfloat-equal. Measured on this tree they produce
    # 262 hits, essentially all of them intentional float/int arithmetic in
    # JUCE-idiomatic graphics and DSP code (`(float) i / count`, passing a float
    # to a double-precision std:: function). Silencing them would mean 262 casts
    # that change no behaviour and hide the few that might matter. The set below
    # is strict and reaches a genuine zero.
    set(FOURCOLOR_WARNING_FLAGS
        -Wall -Wextra -Wpedantic -Wshadow -Wcast-align -Wunused
        -Woverloaded-virtual -Wnon-virtual-dtor -Wimplicit-fallthrough)
    if(FOURCOLOR_WERROR)
        list(APPEND FOURCOLOR_WARNING_FLAGS -Werror)
    endif()
endif()

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
    #  Without this a VST3 declares only the generic "Fx" subcategory, and
    #  Cubase files it under Other rather than Distortion - which looks exactly
    #  like the plug-in failing to appear if you are looking where the README
    #  told you to look.
    VST3_CATEGORIES "Fx" "Distortion"
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "FourColor")

# Everything except the plugin entry points, shared by the plugin and the tools.
# Files are added here phase by phase as they come into existence.
set(FOURCOLOR_CORE_SOURCES
    Source/Core/PresetLibrary.cpp
    Source/Core/StateMigration.cpp
    Source/Dsp/AutoLevel.cpp
    Source/Dsp/BandProcessor.cpp
    Source/Dsp/BehaviorDetector.cpp
    Source/Dsp/ColorEngine.cpp
    Source/Dsp/Crossover.cpp
    Source/Dsp/FourColorEngine.cpp
    Source/Dsp/HarmonicSpace.cpp
    Source/Dsp/NonlinearStage.cpp
    Source/Dsp/ToneStage.cpp
    Source/Ui/Analyzer.cpp
    Source/Ui/BandCards.cpp
    Source/Ui/BandStrip.cpp
    Source/Ui/GlobalBar.cpp
    Source/Ui/Knob.cpp
    Source/Ui/Theme.cpp
    Source/Ui/TopBar.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

set(FOURCOLOR_OWN_SOURCES
    ${FOURCOLOR_CORE_SOURCES}
    Source/Tools/test_dsp.cpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/screenshot_tool.cpp")
    list(APPEND FOURCOLOR_OWN_SOURCES Source/Tools/screenshot_tool.cpp)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/render_tool.cpp")
    list(APPEND FOURCOLOR_OWN_SOURCES Source/Tools/render_tool.cpp)
endif()

set_source_files_properties(${FOURCOLOR_OWN_SOURCES}
    PROPERTIES COMPILE_OPTIONS "${FOURCOLOR_WARNING_FLAGS}")

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

# --- offline renderer / preset fingerprint -------------------------------------
#     build/FourColorRender_artefacts/<config>/FourColorRender --fingerprint
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/render_tool.cpp")
    juce_add_console_app(FourColorRender PRODUCT_NAME "FourColorRender")
    juce_generate_juce_header(FourColorRender)
    target_sources(FourColorRender PRIVATE Source/Tools/render_tool.cpp ${FOURCOLOR_CORE_SOURCES})
    target_compile_definitions(FourColorRender PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(FourColorRender
        PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags)
endif()

# --- sanitiser build of the test suite (opt-in) --------------------------------
#     cmake -B build-san -DFOURCOLOR_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
#     build-san/FourColorTestsSan_artefacts/Debug/FourColorTestsSan
#
# ASan + UBSan on Clang/GCC; MSVC has /fsanitize=address but no UBSan, so the
# Windows build gets ASan only. Kept as a separate target because sanitisers
# make the suite roughly 8x slower and must never be the default.
if(FOURCOLOR_SANITIZERS)
    juce_add_console_app(FourColorTestsSan PRODUCT_NAME "FourColorTestsSan")
    juce_generate_juce_header(FourColorTestsSan)
    target_sources(FourColorTestsSan PRIVATE Source/Tools/test_dsp.cpp ${FOURCOLOR_CORE_SOURCES})
    target_compile_definitions(FourColorTestsSan PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(FourColorTestsSan
        PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags)

    if(MSVC)
        target_compile_options(FourColorTestsSan PRIVATE /fsanitize=address)
    else()
        target_compile_options(FourColorTestsSan PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(FourColorTestsSan PRIVATE -fsanitize=address,undefined)
    endif()
endif()
