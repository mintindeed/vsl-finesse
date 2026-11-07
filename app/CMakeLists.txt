juce_add_gui_app (VSLFinesseApp
    PRODUCT_NAME "VSL Finesse"
    COMPANY_NAME "mintindeed"
    BUNDLE_ID "io.github.mintindeed.vslfinesse"
    VERSION "${PROJECT_VERSION}"
    DOCUMENT_EXTENSIONS mid midi vslfsession vslpsession
    HARDENED_RUNTIME_ENABLED FALSE
    APP_SANDBOX_ENABLED FALSE)

target_sources (VSLFinesseApp PRIVATE
    Source/AppContext.cpp
    Source/AudioEngine.cpp
    Source/DynamicsBar.cpp
    Source/InspectorPanel.cpp
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/PianoRollView.cpp
    Source/PlaybackSequence.cpp
    Source/ProfileLibrary.cpp
    Source/Session.cpp
    Source/TrackListView.cpp)

target_compile_definitions (VSLFinesseApp PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:VSLFinesseApp,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:VSLFinesseApp,JUCE_VERSION>"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=$<IF:$<PLATFORM_ID:Darwin>,1,0>
    JUCE_PLUGINHOST_LV2=0)

target_link_libraries (VSLFinesseApp PRIVATE
    vslf_io
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
