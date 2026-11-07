cmake_minimum_required (VERSION 3.22)

project (VSLPerformer VERSION 0.3.1 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

option (VSLP_BUILD_APP   "Build the VSL Performer app"         ON)
option (VSLP_BUILD_CLI   "Build the vslp-finesse command line tool" ON)
option (VSLP_BUILD_TESTS "Build the unit tests"                ON)

# The engine is plain C++ with no dependencies.
add_subdirectory (engine)

# Everything else uses JUCE for MIDI files, JSON, audio and the GUI.
if (VSLP_BUILD_APP OR VSLP_BUILD_CLI OR VSLP_BUILD_TESTS)
    include (FetchContent)
    set (FETCHCONTENT_QUIET OFF)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable (JUCE)

    add_subdirectory (io)
endif()

if (VSLP_BUILD_CLI)
    add_subdirectory (cli)
endif()

if (VSLP_BUILD_APP)
    add_subdirectory (app)
endif()

if (VSLP_BUILD_TESTS)
    enable_testing()
    add_subdirectory (tests)
endif()
