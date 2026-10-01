include(FetchContent)

if (NOT TARGET Lua::Library)
    find_package(Lua 5.3 QUIET)
endif()

if (NOT Lua_FOUND AND NOT TARGET Lua::Library)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 4.0 AND
        (NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.5))
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()
    set(LUA_BUILD_BINARY OFF CACHE BOOL "" FORCE)
    set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
    set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/walterschell/Lua.git
        GIT_TAG v5.4.6
    )
    FetchContent_MakeAvailable(lua)
endif()

add_library(lambui_lua
    src/LuaBindings.cpp
)
add_library(LambUI::lua ALIAS lambui_lua)

target_include_directories(lambui_lua
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lambui_lua PUBLIC lambui)
target_compile_features(lambui_lua PUBLIC cxx_std_14)
generate_export_header(lambui_lua
    EXPORT_MACRO_NAME LAMBUI_LUA_API
    EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/lambui_lua_export.h
)

if (MSVC)
    target_compile_options(lambui_lua PRIVATE /W4)
else()
    target_compile_options(lambui_lua PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (Lua_FOUND AND NOT TARGET Lua::Library)
    target_link_libraries(lambui_lua PUBLIC ${LUA_LIBRARIES})
    target_include_directories(lambui_lua PUBLIC ${LUA_INCLUDE_DIR})
else()
    target_link_libraries(lambui_lua PUBLIC Lua::Library)
endif()
