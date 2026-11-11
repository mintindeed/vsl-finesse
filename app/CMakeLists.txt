juce_add_gui_app (VSLFinesseApp
    PRODUCT_NAME "VSL Finesse"
    COMPANY_NAME "mintindeed"
    BUNDLE_ID "io.github.mintindeed.vslfinesse"
    VERSION "${PROJECT_VERSION}"
    DOCUMENT_EXTENSIONS mid midi vslfsession vslpsession
    # The app icon. With Xcode 26 or later, actool compiles the Icon Composer
    # bundle (what macOS 26 shows) and an .icns for earlier macOS. With an older
    # Xcode, JUCE makes the .icns from Icon.svg. No ICON_SMALL: JUCE stores a
    # 32-pixel icon as 'icp5', which macOS 26 misreads. See docs/DEVELOPMENT.md.
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.svg"
    ICON_COMPOSER_BUNDLE "${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.icon"
    HARDENED_RUNTIME_ENABLED FALSE
    APP_SANDBOX_ENABLED FALSE
    # Shown in macOS's own permission prompts. Instrument plug-ins run inside
    # the app, so most of these are asked on Synchron Player's behalf.
    # No semicolons in the text: CMake splits the string at them.
    PLIST_TO_MERGE [[
<plist>
  <dict>
    <key>NSRemovableVolumesUsageDescription</key>
    <string>Synchron Player, which runs inside VSL Finesse, reads its samples from your Vienna library, often on an external drive.</string>
    <key>NSDocumentsFolderUsageDescription</key>
    <string>VSL Finesse saves sessions in Documents unless you choose another folder, and Synchron Player may keep presets and settings there.</string>
    <key>NSDesktopFolderUsageDescription</key>
    <string>VSL Finesse opens MIDI files and sessions you keep on the Desktop, and saves instrument details there when you ask.</string>
    <key>NSDownloadsFolderUsageDescription</key>
    <string>VSL Finesse opens MIDI files and sessions you keep in Downloads.</string>
    <key>NSNetworkVolumesUsageDescription</key>
    <string>VSL Finesse opens MIDI files and sessions, and Synchron Player reads samples, from network drives.</string>
    <key>NSLocalNetworkUsageDescription</key>
    <string>VSL Finesse doesn't use the network itself. Vienna's software inside it asks for this. You can deny it.</string>
  </dict>
</plist>]])

# JUCE makes its .icns while CMake configures, so editing Icon.svg has to
# configure again.
set_property (DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.svg")

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
    Source/TrackListView.cpp
    Source/WelcomeOverlay.cpp)

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
