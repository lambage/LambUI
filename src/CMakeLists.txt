include(GenerateExportHeader)

if (LAMBUI_USE_BOOST)
    find_package(Boost 1.85 CONFIG QUIET COMPONENTS optional algorithm)
    if (NOT Boost_FOUND)
        # Not in lambui.xml (Boost is opt-in); expects a boostorg/boost checkout
        # with submodules at ${LAMBUI_DEPS_DIR}/boost.
        set(BOOST_INCLUDE_LIBRARIES optional algorithm)
        set(BOOST_SKIP_INSTALL_RULES OFF)
        lambui_add_dependency(boost TARGET Boost::optional)
        if (LAMBUI_INSTALL)
            install(SCRIPT "${CMAKE_BINARY_DIR}/_deps/boost-build/cmake_install.cmake")
        endif()
    endif()
endif()

# Header-only; vendored purely for FontAtlas.cpp's SDF glyph baking (private
# implementation detail - stb_truetype.h never appears in a public header).
lambui_add_dependency(stb NO_ADD_SUBDIRECTORY)

# Public dependency: UILog.h's Writef() template exposes fmt::format_string,
# so consumers including LambUI headers need fmt on their include path too.
lambui_add_dependency(fmt TARGET fmt::fmt)

add_library(lambui
    UILayoutSolver.cpp
    UIWidget.cpp
    UIControl.cpp
    UICheckBox.cpp
    UIRadioButton.cpp
    UIWindow.cpp
    UIButton.cpp
    UISlider.cpp
    UIProgressBar.cpp
    UITabControl.cpp
    UITreeView.cpp
    UITooltip.cpp
    UIContextMenu.cpp
    UIMenuBar.cpp
    UIInputBox.cpp
    UIDropDownBox.cpp
    UITextureWidget.cpp
    UIImageWidget.cpp
    UITextureAtlas.cpp
    UITextWidget.cpp
    UICanvasWidget.cpp
    UIScrollContainer.cpp
    UIFontAtlas.cpp
    UILog.cpp
    UIManager.cpp
)
add_library(LambUI::lambui ALIAS lambui)

target_include_directories(lambui PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(lambui PUBLIC fmt::fmt)

if (LAMBUI_USE_BOOST)
    target_link_libraries(lambui PUBLIC Boost::optional Boost::algorithm)
    target_compile_definitions(lambui PUBLIC LAMBUI_USE_BOOST)
    target_compile_features(lambui PUBLIC cxx_std_14)
else()
    # std::optional needs C++17; boost::optional (LAMBUI_USE_BOOST=ON) is the
    # only path that keeps this target buildable as C++14.
    target_compile_features(lambui PUBLIC cxx_std_17)
endif()

target_include_directories(lambui
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

generate_export_header(lambui
    BASE_NAME lambui
    EXPORT_MACRO_NAME LAMBUI_API
    EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/lambui_export.h
)

if (MSVC)
    target_compile_options(lambui PRIVATE /W4)
else()
    target_compile_options(lambui PRIVATE -Wall -Wextra -Wpedantic)
endif()
