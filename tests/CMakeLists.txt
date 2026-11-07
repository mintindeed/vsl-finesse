# doctest is header-only. Its own CMakeLists.txt is too old for CMake 4, so
# SOURCE_SUBDIR points at a folder without one: the sources are downloaded but
# not added as a subproject.
FetchContent_Declare (doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  header-only)
FetchContent_MakeAvailable (doctest)

juce_add_console_app (vslf_tests PRODUCT_NAME "vslf-tests")

target_sources (vslf_tests PRIVATE
    TestMain.cpp
    TestHelpers.h
    EngineTests.cpp
    IoTests.cpp)

target_include_directories (vslf_tests PRIVATE "${doctest_SOURCE_DIR}")

target_compile_definitions (vslf_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries (vslf_tests PRIVATE
    vslf_io
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_recommended_config_flags)

add_test (NAME vslf_tests COMMAND vslf_tests)

set_target_properties (vslf_tests PROPERTIES OUTPUT_NAME "vslf-tests")
