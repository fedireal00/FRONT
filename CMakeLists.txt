cmake_minimum_required(VERSION 3.22)

# Build a Universal Binary on macOS (Intel + Apple Silicon) so the plugin
# loads in any DAW regardless of the host architecture (e.g. arm64 Pro Tools
# would otherwise reject an Intel-only build).
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS universal binary architectures" FORCE)
endif()

project(FRONT VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

juce_add_plugin(FRONT
    COMPANY_NAME "fedireal"
    PLUGIN_MANUFACTURER_CODE Pnsd
    PLUGIN_CODE Frnt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "FRONT"
    BUNDLE_ID "com.fedireal.front"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(FRONT)

target_sources(FRONT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(FRONT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FRONT PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
