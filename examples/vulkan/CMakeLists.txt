include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

if (NOT TARGET lambui_lua)
    message(STATUS "LambUI: enable LAMBUI_BUILD_LUA_BINDINGS for the Vulkan Lua example")
    return()
endif()

find_program(LAMBUI_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
set(vulkan_shaders)
foreach(shader ui.vert ui.frag field.vert field.frag)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${LAMBUI_GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/src/${shader}" -o "${output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${shader}"
        VERBATIM)
    list(APPEND vulkan_shaders "${output}")
endforeach()

add_executable(lambui_example_vulkan
    src/main.cpp
    src/VulkanExampleRenderer.cpp
    ${vulkan_shaders}
)

target_compile_definitions(lambui_example_vulkan PRIVATE
    LAMBUI_VULKAN_ASSET_DIR="${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(lambui_example_vulkan PRIVATE lambui_lua glfw Vulkan::Vulkan)

configure_file(src/planner.lua "${CMAKE_CURRENT_BINARY_DIR}/planner.lua" COPYONLY)
