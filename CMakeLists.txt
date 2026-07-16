cmake_minimum_required(VERSION 3.28)

# Version must be known before project() so PROJECT_VERSION matches VERSION file.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake)
project(aequitas VERSION ${AEQUITAS_VERSION} LANGUAGES CXX)
aequitas_write_version_header()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AEQUITAS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AEQUITAS_BUILD_RENDERER "Build Vulkan renderer and GUI app" ON)
option(AEQUITAS_DEV "Development build extras (asserts, debug overlays hooks)" OFF)

# Unified output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Multi-config generators (VS): put all configs in the same bin/
foreach(CFG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${CFG} CFG_U)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_U} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_U} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_U} ${CMAKE_BINARY_DIR}/lib)
endforeach()

function(aequitas_set_warnings TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /W4 /permissive-)
        if(AEQUITAS_WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic)
        if(AEQUITAS_WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Vulkan REQUIRED) # headers + glslc; loading via volk (do NOT link Vulkan::Vulkan)
include(${CMAKE_SOURCE_DIR}/cmake/deps.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/shaders.cmake)

# ---------------------------------------------------------------------------
# aequitas_sim — headless economic core (stdlib only)
# ---------------------------------------------------------------------------
add_library(aequitas_sim STATIC
    src/sim/world.cpp
    src/sim/agent.cpp
    src/sim/order_book.cpp
    src/sim/market.cpp
    src/sim/telemetry.cpp
    src/sim/simulation.cpp
)
target_include_directories(aequitas_sim PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${AEQUITAS_VERSION_HEADER_DIR}
)
if(AEQUITAS_DEV)
    target_compile_definitions(aequitas_sim PUBLIC AEQUITAS_DEV=1)
endif()
aequitas_set_warnings(aequitas_sim)

# ---------------------------------------------------------------------------
# Headless CLI (always built)
# ---------------------------------------------------------------------------
add_executable(aequitas_headless src/app/headless_main.cpp)
target_link_libraries(aequitas_headless PRIVATE aequitas_sim)
aequitas_set_warnings(aequitas_headless)

# ---------------------------------------------------------------------------
# Renderer + GUI (optional if Vulkan toolchain incomplete — still find_package above)
# ---------------------------------------------------------------------------
if(AEQUITAS_BUILD_RENDERER)
    add_library(aequitas_render STATIC
        src/render/vk_context.cpp
        src/render/mesh.cpp
        src/render/meshgen.cpp
        src/render/pipeline.cpp
        src/render/postfx.cpp
        src/render/camera.cpp
        src/render/renderer.cpp
    )
    target_include_directories(aequitas_render PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(aequitas_render PUBLIC
        aequitas_sim
        glfw
        volk
        vk-bootstrap::vk-bootstrap
        vma_lib
        glm::glm
    )
    target_compile_definitions(aequitas_render PUBLIC VK_NO_PROTOTYPES)
    aequitas_set_warnings(aequitas_render)

    add_library(aequitas_ui STATIC
        src/ui/hud.cpp
    )
    target_include_directories(aequitas_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(aequitas_ui PUBLIC
        aequitas_sim
        aequitas_render
        imgui_lib
        implot_lib
    )
    aequitas_set_warnings(aequitas_ui)

    add_executable(aequitas src/app/main.cpp)
    target_link_libraries(aequitas PRIVATE aequitas_ui aequitas_render aequitas_sim)
    aequitas_set_warnings(aequitas)
    aequitas_add_shaders(aequitas
        shaders/lit.vert
        shaders/lit.frag
        shaders/fullscreen.vert
        shaders/ssao.frag
        shaders/ssao_blur.frag
        shaders/ssao_composite.frag
    )
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE aequitas_sim)
aequitas_set_warnings(test_order_book)
add_test(NAME order_book COMMAND test_order_book)

add_executable(test_conservation tests/test_conservation.cpp)
target_link_libraries(test_conservation PRIVATE aequitas_sim)
aequitas_set_warnings(test_conservation)
add_test(NAME conservation COMMAND test_conservation)

add_executable(test_determinism tests/test_determinism.cpp)
target_link_libraries(test_determinism PRIVATE aequitas_sim)
aequitas_set_warnings(test_determinism)
add_test(NAME determinism COMMAND test_determinism)

add_executable(test_parity tests/test_parity.cpp)
target_link_libraries(test_parity PRIVATE aequitas_sim)
aequitas_set_warnings(test_parity)
add_test(NAME parity COMMAND test_parity)

message(STATUS "Aequitas ${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}, renderer=${AEQUITAS_BUILD_RENDERER}")
