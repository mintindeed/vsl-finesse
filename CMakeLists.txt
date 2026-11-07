cmake_minimum_required (VERSION 3.22)

project (VSLFinesse VERSION 0.4.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

option (VSLF_BUILD_APP   "Build the VSL Finesse app"         ON)
option (VSLF_BUILD_CLI   "Build the vsl-finesse command line tool" ON)
option (VSLF_BUILD_TESTS "Build the unit tests"                ON)

# The engine is plain C++ with no dependencies.
add_subdirectory (engine)

# Everything else uses JUCE for MIDI files, JSON, audio and the GUI.
if (VSLF_BUILD_APP OR VSLF_BUILD_CLI OR VSLF_BUILD_TESTS)
    include (FetchContent)
    set (FETCHCONTENT_QUIET OFF)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable (JUCE)

    # Zstandard (BSD licence): Synchron Player compresses its saved state with
    # it. Its own CMake project lives in build/cmake; SOURCE_SUBDIR points at a
    # folder without a CMakeLists.txt, so only the sources are downloaded and
    # the library is built from them here.
    FetchContent_Declare (zstd
        GIT_REPOSITORY https://github.com/facebook/zstd.git
        GIT_TAG        v1.5.6
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  doc)
    FetchContent_MakeAvailable (zstd)
    file (GLOB VSLF_ZSTD_SOURCES
        "${zstd_SOURCE_DIR}/lib/common/*.c"
        "${zstd_SOURCE_DIR}/lib/compress/*.c"
        "${zstd_SOURCE_DIR}/lib/decompress/*.c")
    add_library (vslf_zstd STATIC ${VSLF_ZSTD_SOURCES})
    target_include_directories (vslf_zstd PUBLIC "${zstd_SOURCE_DIR}/lib")
    # No assembly (it doesn't suit universal macOS builds) and no threads.
    target_compile_definitions (vslf_zstd PRIVATE ZSTD_DISABLE_ASM ZSTD_MULTITHREAD_SUPPORT_DEFAULT=0)
    set_target_properties (vslf_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_subdirectory (io)
endif()

if (VSLF_BUILD_CLI)
    add_subdirectory (cli)
endif()

if (VSLF_BUILD_APP)
    add_subdirectory (app)
endif()

if (VSLF_BUILD_TESTS)
    enable_testing()
    add_subdirectory (tests)
endif()
