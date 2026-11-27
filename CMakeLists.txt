# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Stephane P. Pericat
cmake_minimum_required(VERSION 3.22)
project(WILDBAND VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# JUCE 9.0.5, pinned by commit (the same pin as AFTERIMAGE).
# Offline: -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE checked out at this commit.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        d60b7a78030661574a615b586d9325ba7a7fa7dd # 9.0.5
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

set(WILDBAND_DEFS
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    # stream decoding is done by macOS (AudioToolbox); JUCE's own codecs are not compiled in
    JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_OPUS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_USE_WEBP=0)

# ---- optional: the Rings resonator experiment (docs/p1/RINGS-EXPERIMENT.md). Rings is MIT-licensed upstream code by
# Emilie Gillet (Mutable Instruments). It is NOT in this repository: give a checkout of pichenettes/eurorack at the
# pinned commit (with its stmlib submodule) as -DEURORACK_DIR=..., or let CMake fetch it with -DWILDBAND_FETCH_RINGS=ON.
set(WILDBAND_EURORACK_COMMIT f4cfa7697095a28d292bff758beb51deb49b5a86)   # stmlib submodule: e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
option(WILDBAND_FETCH_RINGS "Fetch upstream Rings (pichenettes/eurorack) at the pinned commit" OFF)
if(WILDBAND_FETCH_RINGS AND NOT EURORACK_DIR)
    FetchContent_Declare(eurorack GIT_REPOSITORY https://github.com/pichenettes/eurorack.git GIT_TAG ${WILDBAND_EURORACK_COMMIT} GIT_SUBMODULES stmlib)
    FetchContent_Populate(eurorack)
    set(EURORACK_DIR ${eurorack_SOURCE_DIR})
endif()
if(EURORACK_DIR)
    add_library(wildband_rings STATIC
        ${EURORACK_DIR}/rings/dsp/part.cc ${EURORACK_DIR}/rings/dsp/resonator.cc ${EURORACK_DIR}/rings/dsp/string.cc
        ${EURORACK_DIR}/rings/dsp/fm_voice.cc ${EURORACK_DIR}/rings/dsp/string_synth_part.cc ${EURORACK_DIR}/rings/resources.cc
        ${EURORACK_DIR}/stmlib/dsp/units.cc ${EURORACK_DIR}/stmlib/utils/random.cc)
    target_include_directories(wildband_rings SYSTEM PUBLIC ${EURORACK_DIR})
    target_compile_definitions(wildband_rings PUBLIC TEST WILDBAND_HAS_RINGS=1)   # TEST: upstream's own switch for host builds
    target_compile_options(wildband_rings PRIVATE -w -O2)                        # upstream is compiled unmodified, as it is
    message(STATUS "WILDBAND: Rings experiment enabled from ${EURORACK_DIR}")
endif()

# ---- the instrument
juce_add_gui_app(WILDBAND
    PRODUCT_NAME "WILDBAND"
    COMPANY_NAME "Shortwav Labs"
    BUNDLE_ID com.shortwavlabs.wildband
    # Deliberately NO microphone usage text here: an app that declares microphone use is shown the macOS microphone
    # prompt as soon as it opens an audio device that has inputs, even for playback. ROOM lives in the helper below.
    # many stations still serve plain http://
    PLIST_TO_MERGE "<plist><dict><key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/></dict></dict></plist>")
target_sources(WILDBAND PRIVATE src/Main.cpp src/Radio.cpp src/MicPermission.mm src/AudioIO.cpp)
target_compile_definitions(WILDBAND PRIVATE ${WILDBAND_DEFS} JUCE_APPLICATION_NAME_STRING="WILDBAND" JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/knob/knob.png)
    juce_add_binary_data(wildband_assets SOURCES assets/knob/knob.png)
    target_link_libraries(WILDBAND PRIVATE wildband_assets)
    target_compile_definitions(WILDBAND PRIVATE WILDBAND_HAS_KNOB=1)
endif()
target_link_libraries(WILDBAND PRIVATE juce::juce_audio_utils "-framework AudioToolbox" "-framework AVFoundation" "-framework CoreAudio"
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---- "WILDBAND Room": the microphone helper. Its own bundle id, its own usage text, its own macOS permission.
add_executable(wildband_room MACOSX_BUNDLE src/RoomHelper.mm src/AudioIO.cpp src/MicPermission.mm)
set_target_properties(wildband_room PROPERTIES OUTPUT_NAME "WILDBAND Room" MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RoomHelper.plist)
target_compile_options(wildband_room PRIVATE -Wall -Wextra)
target_link_libraries(wildband_room PRIVATE "-framework AudioToolbox" "-framework CoreAudio" "-framework AVFoundation" "-framework CoreFoundation" "-framework Foundation")

# The helper goes BESIDE the app, not inside it: nested in WILDBAND.app, macOS attributes its microphone request to
# WILDBAND (which declares no microphone use) and kills it (observed; docs/p0/EVIDENCE-LOG.md). Both get an ad-hoc
# signature so macOS can remember the helper's microphone decision. Not a Developer ID.
add_dependencies(WILDBAND wildband_room)
add_custom_command(TARGET WILDBAND POST_BUILD VERBATIM
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_BUNDLE_DIR:WILDBAND>/../WILDBAND Room.app"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:wildband_room>" "$<TARGET_BUNDLE_DIR:WILDBAND>/../WILDBAND Room.app"
    # AGPL: the licence and third-party notices travel inside the bundle (docs/LICENSING.md)
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" "$<TARGET_BUNDLE_DIR:WILDBAND>/Contents/Resources/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/licenses" "$<TARGET_BUNDLE_DIR:WILDBAND>/Contents/Resources/licenses"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:WILDBAND>/../WILDBAND Room.app"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:WILDBAND>")

# ---- the plugin (AU, VST3): first proof of the shared architecture, docs/p1/PLUGIN-ARCHITECTURE.md.
# An instrument (no audio input). It declares no microphone use. Not signed for distribution, not installed by the build.
option(WILDBAND_PLUGIN "Build the AU and VST3 plugin proof" ON)
if(WILDBAND_PLUGIN)
    juce_add_plugin(WILDBAND_Plugin
        PRODUCT_NAME "WILDBAND"
        COMPANY_NAME "Shortwav Labs"
        BUNDLE_ID com.shortwavlabs.wildband.plugin
        PLUGIN_MANUFACTURER_CODE SwLb
        PLUGIN_CODE Wldb
        FORMATS AU VST3
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        VST3_CATEGORIES Instrument
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(WILDBAND_Plugin PRIVATE src/Plugin.cpp src/Radio.cpp)
    target_compile_definitions(WILDBAND_Plugin PUBLIC ${WILDBAND_DEFS} JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(WILDBAND_Plugin PRIVATE juce::juce_audio_utils "-framework AudioToolbox"
        juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
    if(TARGET wildband_rings)
        target_link_libraries(WILDBAND_Plugin PRIVATE wildband_rings)
    endif()
    foreach(format AU VST3)
        add_custom_command(TARGET WILDBAND_Plugin_${format} POST_BUILD VERBATIM
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:WILDBAND_Plugin_${format}>/Contents/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md" "$<TARGET_BUNDLE_DIR:WILDBAND_Plugin_${format}>/Contents/Resources/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/licenses" "$<TARGET_BUNDLE_DIR:WILDBAND_Plugin_${format}>/Contents/Resources/licenses"
            COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:WILDBAND_Plugin_${format}>")
    endforeach()

    # headless check of the built bundles (loads them through JUCE's hosting code; not a DAW)
    juce_add_console_app(wildband_plugin_check PRODUCT_NAME wildband_plugin_check)
    target_sources(wildband_plugin_check PRIVATE tools/PluginCheck.cpp)
    target_compile_definitions(wildband_plugin_check PRIVATE JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_USE_FLAC=0 JUCE_USE_OGGVORBIS=0 JUCE_USE_OPUS=0 JUCE_USE_MP3AUDIOFORMAT=0 JUCE_USE_WEBP=0)
    target_link_libraries(wildband_plugin_check PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# ---- G1 evidence tool: the real receiver, no audio device
juce_add_console_app(wildband_radio_probe PRODUCT_NAME wildband_radio_probe)
target_sources(wildband_radio_probe PRIVATE tools/RadioProbe.cpp src/Radio.cpp)
target_compile_definitions(wildband_radio_probe PRIVATE ${WILDBAND_DEFS})
target_link_libraries(wildband_radio_probe PRIVATE juce::juce_core "-framework AudioToolbox" juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---- receiver-level tuner tests: real Radio + Engine against tools/stream_test_server.py (tools/run_tuner_check.sh)
juce_add_console_app(wildband_tuner_check PRODUCT_NAME wildband_tuner_check)
target_sources(wildband_tuner_check PRIVATE tools/TunerCheck.cpp src/Radio.cpp)
target_compile_definitions(wildband_tuner_check PRIVATE ${WILDBAND_DEFS})
target_link_libraries(wildband_tuner_check PRIVATE juce::juce_core "-framework AudioToolbox" juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ---- voice lab (docs/p0/VOICE-RESEARCH.md): level-matched renders of every candidate voice. No JUCE.
# Candidates that live in other repositories are compiled from there, in place; nothing of theirs is in this tree.
#   -DTAPESTRY_DIR=/path/to/vcv-tapestry    adds Wyrd's time/filter delay and Drift
#   -DRACK_SDK_DIR=/path/to/Rack-SDK        (with TAPESTRY_DIR) adds Korupt, which uses Rack SDK headers
add_executable(wildband_voice_lab research/VoiceLab.cpp)
target_compile_options(wildband_voice_lab PRIVATE -O2)
if(TAPESTRY_DIR)
    target_sources(wildband_voice_lab PRIVATE ${TAPESTRY_DIR}/src/dsp/generated/wyrd_core.cpp ${TAPESTRY_DIR}/src/dsp/generated/drift_core.cpp ${TAPESTRY_DIR}/src/dsp/generated/vultin.cpp)
    target_include_directories(wildband_voice_lab PRIVATE ${TAPESTRY_DIR}/src ${TAPESTRY_DIR}/src/dsp)
    target_compile_definitions(wildband_voice_lab PRIVATE WILDBAND_LAB_TAPESTRY=1)
    if(RACK_SDK_DIR)
        target_include_directories(wildband_voice_lab SYSTEM PRIVATE ${RACK_SDK_DIR}/include ${RACK_SDK_DIR}/dep/include)
        target_compile_definitions(wildband_voice_lab PRIVATE WILDBAND_LAB_RACK=1)
    endif()
endif()

# ---- engine tests: no JUCE
add_executable(wildband_tests tests/Tests.cpp)
target_compile_options(wildband_tests PRIVATE -Wall -Wextra)
if(TARGET wildband_rings)
    target_link_libraries(wildband_tests PRIVATE wildband_rings)
    target_link_libraries(wildband_voice_lab PRIVATE wildband_rings)
    target_link_libraries(WILDBAND PRIVATE wildband_rings)
endif()
enable_testing()
add_test(NAME wildband_tests COMMAND wildband_tests)

# ---- optional: check Patch.h against TALORA's real Graph (private repository; not needed to build WILDBAND)
if(TALORA_DIR)
    add_executable(wildband_talora_parity tests/TaloraParity.cpp ${TALORA_DIR}/src/engine/Routing.cpp)
    target_include_directories(wildband_talora_parity PRIVATE ${TALORA_DIR}/src/engine)
    add_test(NAME wildband_talora_parity COMMAND wildband_talora_parity)
endif()
