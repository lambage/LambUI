include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

enable_language(C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR lambui-loader-only
)
FetchContent_MakeAvailable(glad)

set(loader_dir "${CMAKE_CURRENT_BINARY_DIR}/glad")
add_custom_command(
    OUTPUT "${loader_dir}/src/glad.c" "${loader_dir}/include/glad/glad.h"
           "${loader_dir}/include/KHR/khrplatform.h"
    COMMAND "${CMAKE_COMMAND}" -E env PYTHONUTF8=1 "${Python3_EXECUTABLE}" -m glad
            --profile=core --api=gl=3.3 --generator=c --extensions= --spec=gl
            --reproducible "--out-path=${loader_dir}"
    WORKING_DIRECTORY "${glad_SOURCE_DIR}"
    COMMENT "Generating the OpenGL 3.3 core loader"
    VERBATIM
)
add_library(lambui_example_glad STATIC "${loader_dir}/src/glad.c")
target_include_directories(lambui_example_glad PUBLIC "${loader_dir}/include")
target_link_libraries(lambui_example_glad PRIVATE ${CMAKE_DL_LIBS})

add_executable(lambui_example_opengl33
    src/main.cpp
    src/GL33ExampleRenderer.cpp
)
target_compile_definitions(lambui_example_opengl33 PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(lambui_example_opengl33 PRIVATE lambui glfw lambui_example_glad)