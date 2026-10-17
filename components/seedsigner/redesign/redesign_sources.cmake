# Redesign source list, shared by every build that includes the Redesign
# (the web runner today; the ESP-IDF component when the device lane lands).
# Mirrors screen_sources.cmake: plain glob in CMake script mode, CONFIGURE_DEPENDS
# otherwise, so a new rd_*.cpp is picked up without editing build files.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB REDESIGN_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fonts/*.c")
else()
    file(GLOB REDESIGN_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/fonts/*.c")
endif()

# Bake tokens.json + prototype.json into the binary (redesign_start_embedded()), so a
# device needs no filesystem to run the prototype. Generated at configure time into the
# build dir; editing either JSON re-runs configure. Skipped in script mode (ESP-IDF's
# requirements pass), which never compiles.
set(REDESIGN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/redesign_generated")
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(MAKE_DIRECTORY "${REDESIGN_GENERATED_DIR}")
    foreach(_rd_name tokens prototype)
        set(_rd_src "${CMAKE_CURRENT_LIST_DIR}/${_rd_name}.json")
        file(READ "${_rd_src}" _rd_json)
        file(WRITE "${REDESIGN_GENERATED_DIR}/rd_${_rd_name}_json.inc"
             "// Generated from redesign/${_rd_name}.json at configure time. Do not edit.\n"
             "R\"rdjson(${_rd_json})rdjson\"\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_rd_src}")
    endforeach()
endif()
