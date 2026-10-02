if (NOT TARGET Lua::Library)
    find_package(Lua 5.3 QUIET)
endif()

if (NOT Lua_FOUND AND NOT TARGET Lua::Library)
    # Upstream lua/lua has no CMake build, so compile the core library here.
    enable_language(C)
    lambui_add_dependency(lua NO_ADD_SUBDIRECTORY)
    file(GLOB lambui_lua_core_sources "${lua_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM lambui_lua_core_sources
        "${lua_SOURCE_DIR}/lua.c"
        "${lua_SOURCE_DIR}/luac.c"
        "${lua_SOURCE_DIR}/onelua.c"
    )
    add_library(lambui_lua_core STATIC ${lambui_lua_core_sources})
    target_include_directories(lambui_lua_core PUBLIC "${lua_SOURCE_DIR}")
    if (UNIX)
        target_link_libraries(lambui_lua_core PRIVATE m ${CMAKE_DL_LIBS})
    endif()
    add_library(Lua::Library ALIAS lambui_lua_core)
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
