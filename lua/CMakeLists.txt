include(FetchContent)

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(sol2)

if (NOT TARGET Lua::Library)
    find_package(Lua QUIET)
endif()

if (NOT Lua_FOUND AND NOT TARGET Lua::Library)
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

target_link_libraries(lambui_lua PUBLIC lambui sol2::sol2)

if (Lua_FOUND AND NOT TARGET Lua::Library)
    target_link_libraries(lambui_lua PUBLIC ${LUA_LIBRARIES})
    target_include_directories(lambui_lua PUBLIC ${LUA_INCLUDE_DIR})
else()
    target_link_libraries(lambui_lua PUBLIC Lua::Library)
endif()
