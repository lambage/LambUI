include(GenerateExportHeader)
include(FetchContent)

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
    UIButton.cpp
    UISlider.cpp
    UIInputBox.cpp
    UIDropDownBox.cpp
    UITextureWidget.cpp
    UITextWidget.cpp
    UICanvasWidget.cpp
    UIFontAtlas.cpp
    UILog.cpp
    UIManager.cpp
)
add_library(LambUI::lambui ALIAS lambui)

target_include_directories(lambui PRIVATE ${stb_SOURCE_DIR})
target_link_libraries(lambui PUBLIC fmt::fmt)

target_compile_features(lambui PUBLIC cxx_std_17)

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
