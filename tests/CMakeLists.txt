set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
lambui_add_dependency(googletest TARGET GTest::gtest_main)

add_executable(lambui_tests
    LayoutSolverTests.cpp
    WidgetHierarchyTests.cpp
    EventRoutingTests.cpp
    ScrollContainerTests.cpp
    CompoundWidgetTests.cpp
)

target_link_libraries(lambui_tests PRIVATE lambui GTest::gtest_main)

if (LAMBUI_BUILD_LUA_BINDINGS)
    target_sources(lambui_tests PRIVATE LuaBindingsTests.cpp)
    target_link_libraries(lambui_tests PRIVATE lambui_lua)
endif()

include(GoogleTest)
gtest_discover_tests(lambui_tests)
