if (CMAKE_VERSION VERSION_GREATER 3.0.0)
    cmake_policy(VERSION 3.0.0)
endif ()

function(add_c_test TEST_NAME)
    set_source_files_properties(${TEST_NAME}.c COMPILE_FLAGS "-ggdb -O1")
    add_executable(${TEST_NAME} ${TEST_NAME}.c)
    target_link_libraries(${TEST_NAME} ${STREAMGEO_LIB_NAME} cmocka)
    add_test(${TEST_NAME} ${TEST_NAME})
endfunction(add_c_test)

function(add_c_benchmark BENCH_NAME)
    set_source_files_properties(${BENCH_NAME}.c COMPILE_FLAGS "-O3")
    add_executable(${BENCH_NAME} ${BENCH_NAME}.c)
    target_link_libraries(${BENCH_NAME} ${STREAMGEO_LIB_NAME})
endfunction(add_c_benchmark)
