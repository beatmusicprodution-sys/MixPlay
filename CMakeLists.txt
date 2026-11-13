cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15"CACHE STRING "" FORCE
project(MixPlay VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
  set(PLUGIN_FORMATS VST3 AU Standalone)
else()
  set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(MixPlay
  COMPANY_NAME "Beat Music"
  PLUGIN_MANUFACTURER_CODE BtMu
  PLUGIN_CODE MxPl
  FORMATS ${PLUGIN_FORMATS}
  PRODUCT_NAME "Mix Play"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE)

target_sources(MixPlay PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Analyzer.cpp)

target_compile_definitions(MixPlay PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_options(MixPlay PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(MixPlay
  PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
