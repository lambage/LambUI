include(GenerateExportHeader)

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
    UIManager.cpp
)
add_library(LambUI::lambui ALIAS lambui)

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
