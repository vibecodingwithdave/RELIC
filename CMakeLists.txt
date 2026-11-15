cmake_minimum_required(VERSION 3.22)
project(Relic VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
# Windows: link the C++ runtime statically so the plugin needs no Visual C++ redistributable installed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Audio Units exist only on macOS
if(APPLE)
    set(RELIC_FORMATS VST3 AU Standalone)
else()
    set(RELIC_FORMATS VST3 Standalone)
endif()

add_subdirectory(third_party/JUCE)

juce_add_plugin(Relic
    COMPANY_NAME "McBABE Labs"
    PLUGIN_MANUFACTURER_CODE Rfgd
    PLUGIN_CODE Rfs1
    FORMATS ${RELIC_FORMATS}
    PRODUCT_NAME "Relic"
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon/relic_icon.png
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth)

file(GLOB FACTORY_PRESETS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/presets/*.xml)
juce_add_binary_data(RelicAssets SOURCES
    ${FACTORY_PRESETS}
    Assets/skin/bg_plate.png
    Assets/skin/logo.png
    Assets/skin/scene_cards.jpg
    Assets/skin/scene_treant.jpg
    Assets/skin/scene_wanderer.jpg
    Assets/skin/scene_troll.jpg
    Assets/skin/scene_orcs.jpg
    Assets/skin/scene_knights.jpg
    Assets/skin/scene_dragon.jpg
    Assets/skin/panel_frame.png
    Assets/skin/btn_sheet.png
    Assets/skin/pad_sheet.png
    Assets/skin/knob_body.png
    Assets/skin/knob_pointer.png
    Assets/skin/screen_frame.png
    Assets/skin/icons_sidebar.png
    Assets/skin/icons_action.png
    Assets/skin/icons_effect.png
    Assets/skin/key_white_up.png
    Assets/skin/key_white_down.png
    Assets/skin/key_black_up.png
    Assets/skin/key_black_down.png
    Assets/skin/meter_segs.png
    Assets/skin/font_atlas.png)

if(MSVC)
    target_compile_options(Relic PRIVATE /utf-8 /bigobj)
endif()

target_sources(Relic PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Relic PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Relic
    PRIVATE RelicAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
