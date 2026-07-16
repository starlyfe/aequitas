include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------------------
# GLFW 3.4
# ---------------------------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# volk 1.4.350
# ---------------------------------------------------------------------------
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        1.4.350
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# vk-bootstrap v1.4.350
# ---------------------------------------------------------------------------
FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.4.350
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# VulkanMemoryAllocator v3.4.0
# ---------------------------------------------------------------------------
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Dear ImGui v1.92.8 (docking not required)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# ImPlot — pin master tip for ImGui 1.92+ compatibility (v1.0 is too old)
# SHA recorded in docs/DEVLOG.md
# ---------------------------------------------------------------------------
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        d65a2bef53d32502407de3a4be80f191e2f412d7
)

# ---------------------------------------------------------------------------
# GLM 1.0.1
# ---------------------------------------------------------------------------
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw volk vkbootstrap vma imgui implot glm)

# ---------------------------------------------------------------------------
# ImGui as a static library (GLFW + Vulkan backends, volk path)
# ---------------------------------------------------------------------------
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_lib PUBLIC
    IMGUI_IMPL_VULKAN_USE_VOLK
    IMGUI_DEFINE_MATH_OPERATORS
)
# 32-bit draw indices for dense ImPlot widgets
target_compile_definitions(imgui_lib PUBLIC ImDrawIdx=unsigned\ int)
target_link_libraries(imgui_lib PUBLIC glfw volk)

# ---------------------------------------------------------------------------
# ImPlot as a static library
# ---------------------------------------------------------------------------
add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot_lib PUBLIC imgui_lib)

# ---------------------------------------------------------------------------
# VMA header-only interface
# ---------------------------------------------------------------------------
add_library(vma_lib INTERFACE)
target_include_directories(vma_lib INTERFACE ${vma_SOURCE_DIR}/include)
