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
