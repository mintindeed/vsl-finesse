# Built-in instrument profiles, compiled in.
file (GLOB VSLF_PROFILE_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/profiles/*.json")
juce_add_binary_data (vslf_profiles
    NAMESPACE VslfProfiles
    HEADER_NAME VslfProfiles.h
    SOURCES ${VSLF_PROFILE_FILES})
set_target_properties (vslf_profiles PROPERTIES POSITION_INDEPENDENT_CODE ON)

# MIDI file and JSON support. An INTERFACE library: the sources are compiled
# into each target that uses them, alongside that target's JUCE modules.
add_library (vslf_io INTERFACE)
target_sources (vslf_io INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/BuiltinProfiles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/JsonIO.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MidiIO.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SynchronState.cpp")
target_include_directories (vslf_io INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries (vslf_io INTERFACE vslf_engine vslf_profiles vslf_zstd)
