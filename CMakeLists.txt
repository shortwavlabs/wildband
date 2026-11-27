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
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:WILDBAND>/../WILDBAND Room.app"
    COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:WILDBAND>")

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
enable_testing()
add_test(NAME wildband_tests COMMAND wildband_tests)

# ---- optional: check Patch.h against TALORA's real Graph (private repository; not needed to build WILDBAND)
if(TALORA_DIR)
    add_executable(wildband_talora_parity tests/TaloraParity.cpp ${TALORA_DIR}/src/engine/Routing.cpp)
    target_include_directories(wildband_talora_parity PRIVATE ${TALORA_DIR}/src/engine)
    add_test(NAME wildband_talora_parity COMMAND wildband_talora_parity)
endif()
