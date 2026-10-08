cmake_minimum_required(VERSION 3.22)
project(WhatKey VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Set by build.sh: which Python runs whatkey.py, where it is, and a PATH that finds ffmpeg.
set(WHATKEY_PYTHON "python3" CACHE STRING "Python interpreter with numpy")
set(WHATKEY_SCRIPT "" CACHE STRING "Absolute path to whatkey.py")
set(WHATKEY_PATH "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin" CACHE STRING "PATH for ffmpeg")

juce_add_plugin(WhatKey
    COMPANY_NAME "neutrinoevent"
    PLUGIN_MANUFACTURER_CODE Neut
    PLUGIN_CODE Wkey
    FORMATS VST3 AU
    PRODUCT_NAME "WhatKey"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(WhatKey PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(WhatKey PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    WHATKEY_PYTHON="${WHATKEY_PYTHON}"
    WHATKEY_SCRIPT="${WHATKEY_SCRIPT}"
    WHATKEY_PATH="${WHATKEY_PATH}")

target_link_libraries(WhatKey
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
