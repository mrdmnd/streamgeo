set(CMAKE_MACOSX_RPATH OFF)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type specified, default to release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif ()

project(cstreamgeo)
set(STREAMGEO_LIB_NAME cstreamgeo)

option(BUILD_STATIC "Build a static library" OFF) # turning this on disables production of dynamic library
option(BUILD_LTO "Build library with link-time optimizations" OFF)
option(SANITIZE "Sanitize addresses" OFF)

# Include some of our custom CMake modules/scripts/whatever
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")
find_package(CTargets)
find_package(Options)
find_package(LTO)

include_directories(include /usr/local/include/roaring/)
link_directories(/usr/local/lib/)
install(DIRECTORY include/${STREAMGEO_LIB_NAME} DESTINATION include)

enable_testing()
include(CTest)

# Note: it is Dan Lemire's understanding that these variables
# are passed down to subdirectories, so this needs to be done here.
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/testdata/")
set(BENCHMARK_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/realdata/")
message(STATUS "Using TEST_DATA_DIR: " ${TEST_DATA_DIR})
message(STATUS "Using BENCHMARK_DATA_DIR: " ${BENCHMARK_DATA_DIR})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tests/config.h.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/config.h")

add_subdirectory(src)
add_subdirectory(benchmarks)
add_subdirectory(tests)

# Being terse is good, but knowing how the build is configured 
# is important and should not be hard to figure out.
MESSAGE(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
MESSAGE(STATUS "BUILD_STATIC: " ${BUILD_STATIC})
MESSAGE(STATUS "BUILD_LTO: " ${BUILD_LTO})
MESSAGE(STATUS "SANITIZE: " ${SANITIZE})
MESSAGE(STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER})
MESSAGE(STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS})
MESSAGE(STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG})
MESSAGE(STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE})
