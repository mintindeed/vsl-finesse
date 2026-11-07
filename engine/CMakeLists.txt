add_library (vslf_engine STATIC
    src/Analysis.cpp
    src/Articulations.cpp
    src/Blends.cpp
    src/Controllers.cpp
    src/Dynamics.cpp
    src/Engine.cpp
    src/Ingest.cpp
    src/Pauses.cpp
    src/Performance.cpp
    src/Profile.cpp
    src/TempoMap.cpp
    src/Timing.cpp)

target_include_directories (vslf_engine PUBLIC include)
target_compile_features (vslf_engine PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options (vslf_engine PRIVATE /W4)
else()
    target_compile_options (vslf_engine PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion)
endif()
