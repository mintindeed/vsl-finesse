# Built-in instrument profiles, compiled in.
file (GLOB VSLP_PROFILE_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/profiles/*.json")
juce_add_binary_data (vslp_profiles
    NAMESPACE VslpProfiles
    HEADER_NAME VslpProfiles.h
    SOURCES ${VSLP_PROFILE_FILES})
set_target_properties (vslp_profiles PROPERTIES POSITION_INDEPENDENT_CODE ON)

# MIDI file and JSON support. An INTERFACE library: the sources are compiled
# into each target that uses them, alongside that target's JUCE modules.
add_library (vslp_io INTERFACE)
target_sources (vslp_io INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/BuiltinProfiles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/JsonIO.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MidiIO.cpp")
target_include_directories (vslp_io INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries (vslp_io INTERFACE vslp_engine vslp_profiles)
