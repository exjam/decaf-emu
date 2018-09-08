project(gsh-to-spirv)

include_directories(".")
include_directories("../../src/libdecaf/src")

file(GLOB_RECURSE SOURCE_FILES *.cpp)
file(GLOB_RECURSE HEADER_FILES *.h)

add_executable(gsh-to-spirv ${SOURCE_FILES} ${HEADER_FILES})
set_target_properties(gsh-to-spirv PROPERTIES FOLDER tools)

target_link_libraries(gsh-to-spirv
    common
    libdecaf
    libgfd
    ${EXCMD_LIBRARIES}
    ${SPIRV_LIBRARIES})

install(TARGETS gsh-to-spirv RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
