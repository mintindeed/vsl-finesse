juce_add_console_app (vslf_cli PRODUCT_NAME "vsl-finesse")

target_sources (vslf_cli PRIVATE Main.cpp)

target_compile_definitions (vslf_cli PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (vslf_cli PRIVATE
    vslf_io
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties (vslf_cli PROPERTIES OUTPUT_NAME "vsl-finesse")
