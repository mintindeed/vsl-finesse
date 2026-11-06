FetchContent_Declare (doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable (doctest)

juce_add_console_app (vslp_tests PRODUCT_NAME "vslp-tests")

target_sources (vslp_tests PRIVATE
    TestMain.cpp
    TestHelpers.h
    EngineTests.cpp
    IoTests.cpp)

target_compile_definitions (vslp_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (vslp_tests PRIVATE
    vslp_io
    doctest::doctest
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags)

add_test (NAME vslp_tests COMMAND vslp_tests)

set_target_properties (vslp_tests PROPERTIES OUTPUT_NAME "vslp-tests")
