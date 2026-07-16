# Compile GLSL shaders to SPIR-V next to the target binary.
# Usage: aequitas_add_shaders(<target> shaders/foo.vert shaders/foo.frag ...)
function(aequitas_add_shaders TARGET)
    if(NOT Vulkan_GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc not found. Install the LunarG Vulkan SDK and set VULKAN_SDK.")
    endif()

    set(SPV_OUTPUTS "")
    foreach(SHADER_SRC IN LISTS ARGN)
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SPV_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
        get_filename_component(SPV_DIR ${SPV_OUT} DIRECTORY)
        file(MAKE_DIRECTORY ${SPV_DIR})

        add_custom_command(
            OUTPUT ${SPV_OUT}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} -O -o ${SPV_OUT}
                    ${CMAKE_SOURCE_DIR}/${SHADER_SRC}
            DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER_SRC}
            COMMENT "Compiling ${SHADER_SRC} -> ${SHADER_NAME}.spv"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS ${SPV_OUT})
    endforeach()

    add_custom_target(${TARGET}_shaders DEPENDS ${SPV_OUTPUTS})
    add_dependencies(${TARGET} ${TARGET}_shaders)

    # Copy .spv next to the executable so runtime can find shaders/
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${TARGET}>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_BINARY_DIR}/shaders
                $<TARGET_FILE_DIR:${TARGET}>/shaders
        COMMENT "Copying SPIR-V shaders next to ${TARGET}"
    )
endfunction()
