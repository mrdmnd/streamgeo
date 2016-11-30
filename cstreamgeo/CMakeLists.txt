cmake_minimum_required(VERSION 3.5)
project(cstreamgeo)

# Remove me later!!
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, default to release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Use the package PkgConfig to detect GTK+ headers and library files:
# The GTK dependency is used for the GUI testing app.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(include /usr/local/include/roaring/ ${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

set(CSTREAMGEO_LIB_NAME cstreamgeo)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -std=c11 -mfpmath=sse")

add_library(cstreamgeo SHARED src/similarity.c src/roaringmask.c src/stream.c src/filters.c)
target_link_libraries(cstreamgeo m roaring)

add_executable(similarity_test test/similarity_test.c)
target_link_libraries(similarity_test cstreamgeo)

add_executable(filter_test test/filter_test.c)
target_link_libraries(filter_test cstreamgeo)

add_executable(stream_test test/stream_test.c)
target_link_libraries(stream_test cstreamgeo)

add_executable(gui test/gui.c)
target_link_libraries(gui ${GTK3_LIBRARIES} cstreamgeo)

install(DIRECTORY include/ DESTINATION include/cstreamgeo)
install(TARGETS ${CSTREAMGEO_LIB_NAME} DESTINATION lib)
