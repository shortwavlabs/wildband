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
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "WILDBAND listens to the room to shape its sound. The microphone is analysed on this Mac only. It is never played back, stored or sent anywhere."
    # many stations still serve plain http://
    PLIST_TO_MERGE "<plist><dict><key>NSAppTransportSecurity</key><dict><key>NSAllowsArbitraryLoads</key><true/></dict></dict></plist>")
target_sources(WILDBAND PRIVATE src/Main.cpp src/Radio.cpp src/MicPermission.mm)
target_compile_definitions(WILDBAND PRIVATE ${WILDBAND_DEFS} JUCE_APPLICATION_NAME_STRING="WILDBAND" JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets/knob/knob.png)
    juce_add_binary_data(wildband_assets SOURCES assets/knob/knob.png)
    target_link_libraries(WILDBAND PRIVATE wildband_assets)
    target_compile_definitions(WILDBAND PRIVATE WILDBAND_HAS_KNOB=1)
endif()
target_link_libraries(WILDBAND PRIVATE juce::juce_audio_utils "-framework AudioToolbox" "-framework AVFoundation"
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
# ad-hoc signature over the finished bundle, so macOS can remember the microphone decision. Not a Developer ID.
add_custom_command(TARGET WILDBAND POST_BUILD VERBATIM COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:WILDBAND>")

# ---- G1 evidence tool: the real receiver, no audio device
juce_add_console_app(wildband_radio_probe PRODUCT_NAME wildband_radio_probe)
target_sources(wildband_radio_probe PRIVATE tools/RadioProbe.cpp src/Radio.cpp)
target_compile_definitions(wildband_radio_probe PRIVATE ${WILDBAND_DEFS})
target_link_libraries(wildband_radio_probe PRIVATE juce::juce_core "-framework AudioToolbox" juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

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
