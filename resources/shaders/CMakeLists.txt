project(shaders)
set(OUTPUT_DIR "${PROJECT_BINARY_DIR}")

set(VULKAN_SHADERS
    gpu7_tiling.comp)

if(DECAF_VULKAN)
    find_program(GLSLANG_VALIDATOR NAMES glslangValidator
                HINTS "${Vulkan_INCLUDE_DIR}/../Bin")

    foreach(SOURCE_FILE ${VULKAN_SHADERS})
        get_filename_component(FILE_NAME ${SOURCE_FILE} NAME)
        set(SPIRV "${OUTPUT_DIR}/${FILE_NAME}.spv")
        add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}/"
            COMMAND ${echo} ${GLSLANG_VALIDATOR} -V "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}" -o "${SPIRV}"
            DEPENDS ${SOURCE_FILE})
        list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    endforeach(SOURCE_FILE)

   bin2c(bin2c_vulkan_shaders ${SPIRV_BINARY_FILES})
   set_target_properties(bin2c_vulkan_shaders PROPERTIES FOLDER resources)
endif()

