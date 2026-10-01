#include "GLExampleRenderer.h"
#include "LambUI/LambUI.h"
#include "LambUI/UIEvent.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>

using namespace LambUI;

namespace {
constexpr const char* TAG = "OpenGLExample";
UIManager* g_uiManager = nullptr;
GLExampleRenderer* g_renderer = nullptr;

void CursorPosCallback(GLFWwindow*, double x, double y) {
    if (g_uiManager) g_uiManager->InjectMouseMove(static_cast<float>(x), static_cast<float>(y));
}

void MouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (!g_uiManager) return;
    MouseButton mapped = MouseButton::Left;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) mapped = MouseButton::Right;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mapped = MouseButton::Middle;
    g_uiManager->InjectMouseButton(mapped, action == GLFW_PRESS);
}

void FramebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    if (g_uiManager) g_uiManager->SetDisplaySize(static_cast<float>(width), static_cast<float>(height));
    if (g_renderer) g_renderer->SetViewportSize(width, height);
}

void CharCallback(GLFWwindow*, unsigned int codepoint) {
    if (g_uiManager) g_uiManager->InjectCharacter(static_cast<char32_t>(codepoint));
}
} // namespace

int main() {
    LambUI::Log::UseDefaultConsoleSink();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    // Leave GLFW_OPENGL_PROFILE at its default (GLFW_OPENGL_ANY_PROFILE): this example
    // relies on legacy immediate-mode GL (glBegin/glVertex), and requesting a specific
    // profile is only valid for context version 3.2+, which would break window creation
    // at the default context version 1.0.

    GLFWwindow* window = glfwCreateWindow(1280, 720, "LambUI - OpenGL Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const auto destroyWindow = [](GLFWwindow* ownedWindow) {
        LAMBUI_LOGT(TAG, "Destroy window and context");
        glfwDestroyWindow(ownedWindow);
        glfwTerminate();
    };
    const std::unique_ptr<GLFWwindow, decltype(destroyWindow)> windowOwner(window, destroyWindow);
    auto renderer = std::make_shared<GLExampleRenderer>();
    g_renderer = renderer.get();
    renderer->SetViewportSize(1280, 720);

    // Demo-only: loads a local system font. Real consumers should ship/point
    // at their own TTF asset; FontAtlas::LoadFromFile takes any TTF/OTF path.
    auto fontAtlas = std::make_shared<FontAtlas>();
    std::shared_ptr<FontAtlasTextMeasurer> textMeasurer;
    if (fontAtlas->LoadFromFile("C:/Windows/Fonts/segoeui.ttf")) {
        renderer->LoadFont(*fontAtlas);
        textMeasurer = std::make_shared<FontAtlasTextMeasurer>(*fontAtlas);
    }

    UIManager uiManager(renderer, textMeasurer);
    g_uiManager = &uiManager;
    uiManager.SetDisplaySize(1280.0f, 720.0f);

    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetCharCallback(window, CharCallback);
    glfwSetKeyCallback(window, [](GLFWwindow*, int key, int, int action, int) {
        LAMBUI_LOGT(TAG, "Key({}, {})", key, action);
        uint32_t scanCode = 0;
        if (key == GLFW_KEY_BACKSPACE) scanCode = ScanCode::Backspace;
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) scanCode = ScanCode::Enter;
        else if (key == GLFW_KEY_ESCAPE) scanCode = ScanCode::Escape;
        else if (key == GLFW_KEY_SPACE) scanCode = ScanCode::Space;
        else if (key == GLFW_KEY_TAB) scanCode = ScanCode::Tab;
        else if (key == GLFW_KEY_LEFT_SHIFT) scanCode = ScanCode::LeftShift;
        else if (key == GLFW_KEY_RIGHT_SHIFT) scanCode = ScanCode::RightShift;
        else if (key == GLFW_KEY_LEFT_CONTROL) scanCode = ScanCode::LeftControl;
        else if (key == GLFW_KEY_RIGHT_CONTROL) scanCode = ScanCode::RightControl;
        else if (key == GLFW_KEY_A) scanCode = ScanCode::A;
        else if (key == GLFW_KEY_LEFT) scanCode = ScanCode::Left;
        else if (key == GLFW_KEY_RIGHT) scanCode = ScanCode::Right;
        else if (key == GLFW_KEY_UP) scanCode = ScanCode::Up;
        else if (key == GLFW_KEY_DOWN) scanCode = ScanCode::Down;
        else if (key == GLFW_KEY_HOME) scanCode = ScanCode::Home;
        else if (key == GLFW_KEY_END) scanCode = ScanCode::End;
        else if (key == GLFW_KEY_DELETE) scanCode = ScanCode::Delete;
        if (g_uiManager && scanCode) g_uiManager->InjectKeyEvent(scanCode, action != GLFW_RELEASE);
    });
    glfwSetScrollCallback(window, [](GLFWwindow*, double xOffset, double yOffset) {
        LAMBUI_LOGT(TAG, "Scroll({}, {})", xOffset, yOffset);
        if (g_uiManager) {
            g_uiManager->InjectMouseWheel(static_cast<float>(xOffset) * 20.0f,
                                         static_cast<float>(yOffset) * 20.0f);
        }
    });

    // --- Build a small demo UI tree to showcase the widgets ---
    UIWidget& root = uiManager.GetRoot();

    UIWidget* panel = root.CreateChild<UIWidget>("Panel");
    panel->SetSize(220.0f, 140.0f);
    panel->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 20.0f, 20.0f);

    UITextureWidget* background = panel->CreateChild<UITextureWidget>("PanelBackground");
    background->SetAllPoints(panel);
    background->SetTint(0xFF2B2B2Bu);

    UIButton* button = panel->CreateChild<UIButton>("DemoButton");
    button->SetSize(180.0f, 40.0f);
    button->SetPoint(AnchorPoint::Top, panel, AnchorPoint::Top, 0.0f, 16.0f);
    button->RegisterCallback(UIEventType::OnClick, [](const UIEventData&) {
        std::cout << "DemoButton clicked!\n";
    });

    UISlider* slider = panel->CreateChild<UISlider>("DemoSlider");
    slider->SetSize(180.0f, 16.0f);
    slider->SetPoint(AnchorPoint::Top, button, AnchorPoint::Bottom, 0.0f, 16.0f);
    slider->SetMinMaxValues(0.0f, 100.0f);
    slider->RegisterCallback(UIEventType::OnValueChanged, [&slider](const UIEventData& data) {
        std::cout << "DemoSlider value changed new value: " << slider->GetValue() << "\n";
    });

    UITextWidget* label = panel->CreateChild<UITextWidget>("DemoLabel");
    label->SetTextMeasurer(uiManager.GetTextMeasurer());
    label->SetText("Hello, LambUI!");
    label->SetPoint(AnchorPoint::Top, slider, AnchorPoint::Bottom, 0.0f, 16.0f);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        uiManager.Update(0.0f);

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        uiManager.Render();

        glfwSwapBuffers(window);
    }

    g_uiManager = nullptr;
    g_renderer = nullptr;
    return 0;
}
