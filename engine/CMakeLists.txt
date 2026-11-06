add_library (vslp_engine STATIC
    src/Analysis.cpp
    src/Controllers.cpp
    src/Dynamics.cpp
    src/Engine.cpp
    src/Ingest.cpp
    src/Profile.cpp
    src/TempoMap.cpp
    src/Timing.cpp)

target_include_directories (vslp_engine PUBLIC include)
target_compile_features (vslp_engine PUBLIC cxx_std_20)

if (MSVC)
    target_compile_options (vslp_engine PRIVATE /W4)
else()
    target_compile_options (vslp_engine PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion)
endif()
