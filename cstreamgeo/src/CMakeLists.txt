if (BUILD_STATIC)
    set(STREAMGEO_LIB_TYPE STATIC)
else ()
    set(STREAMGEO_LIB_TYPE SHARED)
endif ()

set(STREAMGEO_SRC
        roaringmask.c
        stridedmask.c
        similarity.c
        stream.c)

add_library(${STREAMGEO_LIB_NAME} ${STREAMGEO_LIB_TYPE} ${STREAMGEO_SRC})
target_link_libraries(${STREAMGEO_LIB_NAME} m roaring)
install(TARGETS ${STREAMGEO_LIB_NAME} DESTINATION lib)
set_target_properties(${STREAMGEO_LIB_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "..") 
