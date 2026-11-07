cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(ButterflyCompressor VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(BFLY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND BFLY_FORMATS AU)
endif()

juce_add_plugin(ButterflyCompressor
    COMPANY_NAME "iNi"
    BUNDLE_ID "com.ini.butterfly"
    PLUGIN_MANUFACTURER_CODE Inia
    PLUGIN_CODE Bfly
    FORMATS ${BFLY_FORMATS}
    PRODUCT_NAME "Butterfly Compressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Spatial
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ButterflyCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ButterflyCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ButterflyCompressor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: renders docs/screenshot.png with live test audio (cmake -DBFLY_SCREENSHOT=ON)
option(BFLY_SCREENSHOT "Build the screenshot tool" OFF)
if(BFLY_SCREENSHOT)
    juce_add_console_app(BflyScreenshot PRODUCT_NAME "BflyScreenshot")
    target_sources(BflyScreenshot PRIVATE Tools/Screenshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(BflyScreenshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Butterfly Compressor" JucePlugin_Build_Standalone=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(BflyScreenshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
