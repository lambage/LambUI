cmake_minimum_required(VERSION 3.20)

project(LambUI
    VERSION 0.1.0
    DESCRIPTION "An engine-agnostic, retained-mode UI library for games"
    LANGUAGES CXX
)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow LambUI to be used both as a top-level build and as a dependency
# (add_subdirectory/FetchContent) of another CMake project.
set(LAMBUI_IS_TOP_LEVEL OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LAMBUI_IS_TOP_LEVEL ON)
endif()

option(LAMBUI_BUILD_EXAMPLES "Build the LambUI example backends" ${LAMBUI_IS_TOP_LEVEL})
option(LAMBUI_BUILD_TESTS "Build the LambUI unit test suite" ${LAMBUI_IS_TOP_LEVEL})
option(LAMBUI_BUILD_LUA_BINDINGS "Build the Lua C API scripting companion library" OFF)
option(LAMBUI_INSTALL "Generate install/export targets for LambUI" ${LAMBUI_IS_TOP_LEVEL})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(src)

if (LAMBUI_BUILD_LUA_BINDINGS)
    add_subdirectory(lua)
endif()

if (LAMBUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (LAMBUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (LAMBUI_INSTALL)
    include(LambUIInstall)
endif()
