set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

lambui_add_dependency(glfw TARGET glfw)

find_package(OpenGL REQUIRED)

add_executable(lambui_example_opengl
    src/main.cpp
    src/GLExampleRenderer.cpp
)

target_link_libraries(lambui_example_opengl PRIVATE lambui glfw OpenGL::GL)
