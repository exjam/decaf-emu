project(libdecaf)

include_directories(".")
include_directories("src")

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h)

add_library(libdecaf STATIC ${SOURCE_FILES} ${HEADER_FILES})
GroupSources(src)

target_link_libraries(libdecaf
    common
    libcpu
    libgfd
    libgpu
    ${ADDRLIB_LIBRARIES}
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${CEREAL_LIBRARIES}
    ${FIXED_POINT_LIBRARIES}
    ${IMGUI_LIBRARIES}
    ${PUGIXML_LIBRARIES}
    ${SWSCALE_LIBRARY}
    ${ZLIB_LINK})
target_include_directories(libdecaf PRIVATE
    ${AVCODEC_INCLUDE_DIR}
    ${AVFORMAT_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${SWSCALE_INCLUDE_DIR})

if(APPLE)
    target_compile_definitions(libdecaf PUBLIC DECAF_NOGL)
else()
    target_link_libraries(libdecaf
        ${GLBINDING_LIBRARIES}
        ${OPENGL_LIBRARIES})
endif()

if(MSVC)
    target_link_libraries(libdecaf ws2_32)
    target_compile_options(libdecaf PUBLIC /wd4251)
endif()
