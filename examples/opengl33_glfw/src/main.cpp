#include "GL33ExampleRenderer.h"
#include "LambUI/UIButton.h"
#include "LambUI/UICanvasWidget.h"
#include "LambUI/UILog.h"
#include "LambUI/UIManager.h"
#include "LambUI/UISlider.h"
#include "LambUI/UITextureWidget.h"
#include "LambUI/UITextWidget.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace LambUI;

namespace {
constexpr const char* TAG = "OpenGL33Example";

UIManager* Manager(GLFWwindow* window) {
    return static_cast<UIManager*>(glfwGetWindowUserPointer(window));
}

void InstallInput(GLFWwindow* window, UIManager& manager) {
    LAMBUI_LOGT(TAG, "InstallInput");
    glfwSetWindowUserPointer(window, &manager);
    glfwSetCursorPosCallback(window, [](GLFWwindow* source, double mouseX, double mouseY) {
        LAMBUI_LOGT(TAG, "Cursor({}, {})", mouseX, mouseY);
        Manager(source)->InjectMouseMove(static_cast<float>(mouseX), static_cast<float>(mouseY));
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* source, int button, int action, int) {
        LAMBUI_LOGT(TAG, "MouseButton({}, {})", button, action);
        MouseButton mapped;
        if (button == GLFW_MOUSE_BUTTON_LEFT) mapped = MouseButton::Left;
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) mapped = MouseButton::Right;
        else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mapped = MouseButton::Middle;
        else return;
        double mouseX = 0.0, mouseY = 0.0;
        glfwGetCursorPos(source, &mouseX, &mouseY);
        Manager(source)->InjectMouseMove(static_cast<float>(mouseX), static_cast<float>(mouseY));
        Manager(source)->InjectMouseButton(mapped, action == GLFW_PRESS);
    });
    glfwSetScrollCallback(window, [](GLFWwindow* source, double deltaX, double deltaY) {
        LAMBUI_LOGT(TAG, "Scroll({}, {})", deltaX, deltaY);
        Manager(source)->InjectMouseWheel(static_cast<float>(deltaX) * 20.0f,
                                          static_cast<float>(deltaY) * 20.0f);
    });
    glfwSetCharCallback(window, [](GLFWwindow* source, unsigned int codepoint) {
        LAMBUI_LOGT(TAG, "Character({})", codepoint);
        Manager(source)->InjectCharacter(static_cast<char32_t>(codepoint));
    });
    glfwSetKeyCallback(window, [](GLFWwindow* source, int key, int, int action, int) {
        LAMBUI_LOGT(TAG, "Key({}, {})", key, action);
        uint32_t scanCode = 0;
        if (key == GLFW_KEY_BACKSPACE) scanCode = ScanCode::Backspace;
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) scanCode = ScanCode::Enter;
        else if (key == GLFW_KEY_ESCAPE) scanCode = ScanCode::Escape;
        if (scanCode) Manager(source)->InjectKeyEvent(scanCode, action != GLFW_RELEASE);
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(source, GLFW_TRUE);
    });
}

std::vector<unsigned char> ReadPixels(int width, int height) {
    LAMBUI_LOGT(TAG, "ReadPixels({}, {})", width, height);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

bool SaveScreenshot(const std::string& path, int width, int height) {
    LAMBUI_LOGT(TAG, "SaveScreenshot({})", path);
    const auto pixels = ReadPixels(width, height);
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << width << ' ' << height << "\n255\n";
    for (int row = height - 1; row >= 0; --row) {
        for (int column = 0; column < width; ++column) {
            const auto offset = (static_cast<size_t>(row) * width + column) * 4;
            output.write(reinterpret_cast<const char*>(pixels.data() + offset), 3);
        }
    }
    return static_cast<bool>(output);
}

bool CheckRenderer(GL33ExampleRenderer& renderer, int width, int height) {
    LAMBUI_LOGT(TAG, "CheckRenderer({}, {})", width, height);
    renderer.SetViewportSize(64, 64, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    UIRenderCommand outer, inner, fill, pop, marker, callback;
    outer.type = inner.type = RenderCommandType::PushScissor;
    outer.x = outer.y = 8.0f;
    outer.width = outer.height = 48.0f;
    inner.x = inner.y = 24.0f;
    inner.width = inner.height = 48.0f;
    fill.width = fill.height = 64.0f;
    fill.color = 0xE02020FFu;
    pop.type = RenderCommandType::PopScissor;
    marker.width = 16.0f;
    marker.height = 64.0f;
    marker.color = 0x20E020FFu;
    callback.type = RenderCommandType::CustomCallback;
    callback.customRenderFunc = [](const UICustomRenderArgs&) {
        LAMBUI_LOGT(TAG, "Smoke callback changes GL state");
        glUseProgram(0);
        glBindVertexArray(0);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glViewport(0, 0, 1, 1);
    };
    renderer.SubmitRenderCommands({outer, inner, callback, fill, pop, marker, pop});
    auto pixels = ReadPixels(width, height);
    const auto matches = [&](int logicalX, int logicalY, std::array<int, 3> expected) {
        const int pixelX = logicalX * width / 64;
        const int pixelY = height - 1 - logicalY * height / 64;
        const size_t offset = (static_cast<size_t>(pixelY) * width + pixelX) * 4;
        for (size_t channel = 0; channel < expected.size(); ++channel) {
            if (std::abs(static_cast<int>(pixels[offset + channel]) - expected[channel]) > 2) return false;
        }
        return true;
    };
    bool passed = matches(32, 32, {224, 32, 32}) && matches(10, 10, {32, 224, 32}) &&
                  matches(4, 10, {0, 0, 0}) && matches(60, 32, {0, 0, 0});

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    const std::array<unsigned char, 8> texels = {255, 0, 0, 255, 0, 0, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    fill.textureHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(texture));
    fill.color = 0xFFFFFFFFu;
    fill.u0 = fill.u1 = 0.75f;
    renderer.SubmitRenderCommands({fill});
    pixels = ReadPixels(width, height);
    passed = matches(32, 32, {0, 0, 255}) && passed;
    glDeleteTextures(1, &texture);

    glClear(GL_COLOR_BUFFER_BIT);
    UIRenderCommand text;
    text.type = RenderCommandType::DrawString;
    text.text = "SDF";
    renderer.SubmitRenderCommands({text});
    pixels = ReadPixels(width, height);
    size_t ink = 0, antialiased = 0;
    for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        if (pixels[offset] > 0) ++ink;
        if (pixels[offset] > 0 && pixels[offset] < 255) ++antialiased;
    }
    passed = passed && ink > 20 && antialiased > 5 && glGetError() == GL_NO_ERROR;
    LAMBUI_LOGI(TAG, "Renderer smoke: {} (SDF ink={}, antialiased={})", passed ? "PASS" : "FAIL", ink, antialiased);
    return passed;
}

int RunExample(GLFWwindow* window, const std::string& fontPath, bool smoke, const std::string& screenshotPrefix) {
    LAMBUI_LOGT(TAG, "RunExample(smoke={})", smoke);
    auto renderer = std::make_shared<GL33ExampleRenderer>();
    if (!renderer->Initialize()) return 1;
    FontAtlas atlas;
    if (!atlas.LoadFromFile(fontPath, 28) || !renderer->LoadFont(atlas)) {
        LAMBUI_LOGE(TAG, "Cannot load font '{}'; pass --font <path-to-ttf>", fontPath);
        return 1;
    }
    auto measurer = std::make_shared<FontAtlasTextMeasurer>(atlas);
    UIManager manager(renderer, measurer);
    InstallInput(window, manager);
    auto& root = manager.GetRoot();
    auto* canvas = root.CreateChild<UICanvasWidget>("ShaderCanvas");
    canvas->SetAllPoints(&root);
    canvas->SetRenderCallback([&renderer](const UICustomRenderArgs& args) { renderer->DrawEffect(args); });
    auto* header = root.CreateChild<UITextureWidget>("Header");
    header->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft);
    header->SetPoint(AnchorPoint::BottomRight, &root, AnchorPoint::TopRight, 0.0f, 108.0f);
    header->SetTint(0x151B1FF5u);
    const auto label = [&manager](UIWidget& parent, const char* name, const char* text,
                                   float offsetX, float offsetY, uint32_t color) {
        LAMBUI_LOGT(TAG, "CreateLabel({})", name);
        auto* widget = parent.CreateChild<UITextWidget>(name);
        widget->SetMouseEnabled(false);
        widget->SetTextMeasurer(manager.GetTextMeasurer());
        widget->SetText(text);
        widget->SetColor(color);
        widget->SetPoint(AnchorPoint::TopLeft, &parent, AnchorPoint::TopLeft, offsetX, offsetY);
        return widget;
    };
    label(*header, "Title", "LambUI / OpenGL 3.3", 24.0f, 18.0f, 0xF0F4F3FFu);
    label(*header, "Subtitle", "CONTOUR STUDY", 24.0f, 56.0f, 0x67DBB3FFu);
    auto* footer = root.CreateChild<UITextureWidget>("Controls");
    footer->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::BottomLeft, 0.0f, -180.0f);
    footer->SetPoint(AnchorPoint::BottomRight, &root, AnchorPoint::BottomRight);
    footer->SetTint(0x151B1FFFu);
    label(*footer, "Parameter", "Distortion", 24.0f, 16.0f, 0xF0F4F3FFu);
    auto* slider = footer->CreateChild<UISlider>("DistortionSlider");
    slider->SetPoint(AnchorPoint::TopLeft, footer, AnchorPoint::TopLeft, 24.0f, 62.0f);
    slider->SetPoint(AnchorPoint::BottomRight, footer, AnchorPoint::TopRight, -24.0f, 84.0f);
    slider->SetMinMaxValues(0.0f, 1.0f);
    slider->SetValue(0.5f);
    auto* button = footer->CreateChild<UIButton>("PauseButton");
    button->SetSize(148.0f, 44.0f);
    button->SetPoint(AnchorPoint::TopLeft, footer, AnchorPoint::TopLeft, 24.0f, 112.0f);
    button->SetNormalColor(0x286550FFu);
    button->SetHoverColor(0x347F66FFu);
    button->SetPressedColor(0x1B493BFFu);
    auto* buttonText = label(*button, "PauseLabel", "Pause", 18.0f, 6.0f, 0xFFFFFFFFu);
    bool paused = false;
    button->RegisterCallback(UIEventType::OnClick, [&paused, buttonText](const UIEventData&) {
        paused = !paused;
        LAMBUI_LOGT(TAG, "Pause({})", paused);
        buttonText->SetText(paused ? "Resume" : "Pause");
    });

    float effectTime = 0.0f;
    double previousTime = glfwGetTime();
    int previousWidth = -1, previousHeight = -1;
    int smokeFrame = 0;
    bool passed = true;
    bool screenshotSaved = false;
    while (!glfwWindowShouldClose(window)) {
        if (smoke) glfwSetWindowSize(window, smokeFrame == 0 ? 1100 : 420, 720);
        glfwPollEvents();
        int width = 0, height = 0, framebufferWidth = 0, framebufferHeight = 0;
        glfwGetWindowSize(window, &width, &height);
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (width <= 0 || height <= 0 || framebufferWidth <= 0 || framebufferHeight <= 0) {
            if (smoke) return 1;
            glfwWaitEvents();
            previousTime = glfwGetTime();
            continue;
        }
        if (smoke) passed = CheckRenderer(*renderer, framebufferWidth, framebufferHeight) && passed;
        renderer->SetViewportSize(width, height, framebufferWidth, framebufferHeight);
        if (width != previousWidth || height != previousHeight) {
            manager.SetDisplaySize(static_cast<float>(width), static_cast<float>(height));
            previousWidth = width;
            previousHeight = height;
        }
        const double now = glfwGetTime();
        const float deltaTime = static_cast<float>(std::min(now - previousTime, 0.1));
        previousTime = now;
        if (!paused) effectTime += deltaTime;
        manager.Update(deltaTime);
        if (smoke) {
            const auto buttonRect = button->GetComputedRect();
            manager.InjectMouseMove(buttonRect.x + 30.0f, buttonRect.y + 20.0f);
            manager.InjectMouseButton(MouseButton::Left, true);
            manager.InjectMouseButton(MouseButton::Left, false);
            passed = passed && paused;
            manager.InjectMouseButton(MouseButton::Left, true);
            manager.InjectMouseButton(MouseButton::Left, false);
            passed = passed && !paused;
            const auto sliderRect = slider->GetComputedRect();
            manager.InjectMouseMove(sliderRect.x + sliderRect.width * 0.5f, sliderRect.y + 10.0f);
            manager.InjectMouseButton(MouseButton::Left, true);
            manager.InjectMouseMove(sliderRect.x + sliderRect.width * 0.8f, sliderRect.y + 10.0f);
            manager.InjectMouseButton(MouseButton::Left, false);
            passed = passed && std::abs(slider->GetValue() - 0.8f) < 0.05f;
            slider->SetValue(0.5f);
            manager.InjectMouseMove(0.0f, 0.0f);
            manager.Update(0.0f);
            effectTime = 1.0f;
        }
        renderer->SetEffect(effectTime, slider->GetValue());
        glClearColor(0.04f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        manager.Render();
        if (smoke) {
            const auto first = ReadPixels(framebufferWidth, framebufferHeight);
            renderer->SetEffect(effectTime + 0.5f, slider->GetValue());
            manager.Render();
            const auto animated = ReadPixels(framebufferWidth, framebufferHeight);
            renderer->SetEffect(effectTime, 0.9f);
            manager.Render();
            const auto distorted = ReadPixels(framebufferWidth, framebufferHeight);
            passed = passed && first != animated && first != distorted;
            renderer->SetEffect(effectTime, slider->GetValue());
            manager.Render();
            LAMBUI_LOGI(TAG, "Showcase smoke {}x{}: {}", width, height, passed ? "PASS" : "FAIL");
        }
        if (!screenshotPrefix.empty() && (!screenshotSaved || smoke)) {
            const std::string suffix = width < 600 ? "-compact.ppm" : "-desktop.ppm";
            passed = SaveScreenshot(screenshotPrefix + suffix, framebufferWidth, framebufferHeight) && passed;
            screenshotSaved = true;
        }
        if (const GLenum error = glGetError(); error != GL_NO_ERROR) {
            LAMBUI_LOGE(TAG, "OpenGL error: {}", error);
            passed = false;
            break;
        }
        glfwSwapBuffers(window);
        if (smoke && ++smokeFrame == 2) break;
    }
    glfwSetWindowUserPointer(window, nullptr);
    return passed ? 0 : 1;
}
}

int main(int argc, char** argv) {
    Log::UseDefaultConsoleSink();
    Log::SetMinLevel(LogLevel::Info);
    LAMBUI_LOGI(TAG, "Starting OpenGL 3.3 example");
    bool smoke = false;
    std::string fontPath;
    std::string screenshotPrefix;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--smoke-test") smoke = true;
        else if (argument == "--font" && index + 1 < argc) fontPath = argv[++index];
        else if (argument == "--screenshot" && index + 1 < argc) screenshotPrefix = argv[++index];
        else {
            LAMBUI_LOGE(TAG, "Usage: lambui_example_opengl33 [--font path] [--smoke-test] [--screenshot prefix]");
            return 1;
        }
    }
    if (fontPath.empty()) {
        for (const char* candidate : {"C:/Windows/Fonts/segoeui.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                     "/System/Library/Fonts/Supplemental/Arial.ttf"}) {
            if (std::filesystem::exists(candidate)) {
                fontPath = candidate;
                break;
            }
        }
    }
    glfwSetErrorCallback([](int code, const char* message) {
        LAMBUI_LOGE(TAG, "GLFW {}: {}", code, message);
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    if (smoke) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1100, 720, "LambUI - OpenGL 3.3", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) || !GLAD_GL_VERSION_3_3) {
        LAMBUI_LOGE(TAG, "OpenGL 3.3 is unavailable");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    LAMBUI_LOGI(TAG, "GL {} / {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    glfwSetWindowSizeLimits(window, 360, 420, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSwapInterval(smoke ? 0 : 1);
    const int result = RunExample(window, fontPath, smoke, screenshotPrefix);
    LAMBUI_LOGT(TAG, "Destroy window and terminate GLFW");
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}