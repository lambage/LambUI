include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

add_executable(lambui_tests
    LayoutSolverTests.cpp
    WidgetHierarchyTests.cpp
    EventRoutingTests.cpp
    ScrollContainerTests.cpp
    CompoundWidgetTests.cpp
)

target_link_libraries(lambui_tests PRIVATE lambui GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lambui_tests)
