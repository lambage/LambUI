include(GenerateExportHeader)
include(FetchContent)

if (LAMBUI_USE_BOOST)
    find_package(Boost 1.85 CONFIG QUIET COMPONENTS optional algorithm)
    if (NOT Boost_FOUND)
        set(BOOST_INCLUDE_LIBRARIES optional algorithm)
        set(BOOST_SKIP_INSTALL_RULES OFF)
        if (POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()
        FetchContent_Declare(
            Boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz
            URL_HASH SHA256=2c5ec5edcdff47ff55e27ed9560b0a0b94b07bd07ed9928b476150e16b0efc57
        )
        FetchContent_MakeAvailable(Boost)
        if (LAMBUI_INSTALL)
            install(SCRIPT "${boost_BINARY_DIR}/cmake_install.cmake")
        endif()
    endif()
endif()

# Header-only; vendored purely for FontAtlas.cpp's SDF glyph baking (private
# implementation detail - stb_truetype.h never appears in a public header).
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

# Public dependency: UILog.h's Writef() template exposes fmt::format_string,
# so consumers including LambUI headers need fmt on their include path too.
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fmt)

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
