project(resources)

 macro(bin2c TARGET INPUT_FILES)
    set(BIN2C "${CMAKE_SOURCE_DIR}/libraries/bin2c.cmake")
    set(BIN2C_TARGET_DIR "${PROJECT_BINARY_DIR}/${TARGET}")
    set(BIN2C_CMDLINE
        "-DOUTPUT_C=${BIN2C_TARGET_DIR}/${TARGET}.cpp"
        "-DOUTPUT_H=${BIN2C_TARGET_DIR}/${TARGET}.h"
        "-DINPUT_FILES=${INPUT_FILES}"
        -P "${BIN2C}")

    add_custom_command(
        OUTPUT "${BIN2C_TARGET_DIR}/${TARGET}.cpp" "${BIN2C_TARGET_DIR}/${TARGET}.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN2C_TARGET_DIR}"
        COMMAND ${CMAKE_COMMAND} ARGS ${BIN2C_CMDLINE}
        DEPENDS ${INPUT_FILES} ${BIN2C}
        COMMENT "Running bin2c for ${TARGET}")

    add_library(${TARGET} STATIC
       "${BIN2C_TARGET_DIR}/${TARGET}.cpp"
       "${BIN2C_TARGET_DIR}/${TARGET}.h")
    target_include_directories(${TARGET}
       PUBLIC "${BIN2C_TARGET_DIR}")
 endmacro()

set(Fonts
   fonts/CafeCn.ttf
   fonts/CafeKr.ttf
   fonts/CafeStd.ttf
   fonts/CafeTw.ttf
   fonts/NotoSansCJK.LICENSE
   fonts/DejaVuSansMono.ttf
   fonts/DejaVuSansMono.LICENSE)

set(ResourceFiles ${Fonts})

add_custom_target(resources ALL SOURCES ${ResourceFiles})
set_target_properties(resources PROPERTIES FOLDER resources)

foreach(ResourceFile ${ResourceFiles})
    add_custom_command(TARGET resources PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E
                       copy ${CMAKE_CURRENT_SOURCE_DIR}/${ResourceFile}
                            $<TARGET_FILE_DIR:libdecaf>/resources/${ResourceFile})
endforeach()

add_subdirectory(shaders)

install(FILES ${Fonts}
        DESTINATION "${CMAKE_INSTALL_PREFIX}/resources/fonts")
