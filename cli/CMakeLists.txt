juce_add_console_app (vslp_cli PRODUCT_NAME "vslp-finesse")

target_sources (vslp_cli PRIVATE Main.cpp)

target_compile_definitions (vslp_cli PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (vslp_cli PRIVATE
    vslp_io
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties (vslp_cli PROPERTIES OUTPUT_NAME "vslp-finesse")
