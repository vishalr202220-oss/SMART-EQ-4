cmake_minimum_required(VERSION 3.22)
project(SmartEQ VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SmartEQ
    COMPANY_NAME "ProducerToy"
    PLUGIN_MANUFACTURER_CODE Prtd
    PLUGIN_CODE Smeq
    FORMATS VST3 Standalone
    PRODUCT_NAME "SMART EQ PRODUCERTOY"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE)

target_sources(SmartEQ PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(SmartEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SmartEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
