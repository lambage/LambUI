#include "GLExampleRenderer.h"
#include "../../common/WidgetShowcase.h"
#include "LambUI/UILog.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace LambUI;

namespace {
constexpr const char* TAG = "OpenGLExample";
UIManager* g_uiManager = nullptr;

void CursorPosCallback(GLFWwindow*, double x, double y) {
    LAMBUI_LOGT(TAG, "Cursor({}, {})", x, y);
    if (g_uiManager) g_uiManager->InjectMouseMove(static_cast<float>(x), static_cast<float>(y));
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    LAMBUI_LOGT(TAG, "MouseButton({}, {})", button, action);
    if (!g_uiManager) return;
    MouseButton mapped = MouseButton::Left;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) mapped = MouseButton::Right;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mapped = MouseButton::Middle;
    else if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    double mouseX = 0.0, mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    g_uiManager->InjectMouseMove(static_cast<float>(mouseX), static_cast<float>(mouseY));
    g_uiManager->InjectMouseButton(mapped, action == GLFW_PRESS);
}

void CharCallback(GLFWwindow*, unsigned int codepoint) {
    LAMBUI_LOGT(TAG, "Character({})", codepoint);
    if (g_uiManager) g_uiManager->InjectCharacter(static_cast<char32_t>(codepoint));
}

bool CaptureFrame(const std::string& path, int width, int height, std::vector<unsigned char>& pixels) {
    LAMBUI_LOGT(TAG, "CaptureFrame({}, {}x{})", path, width, height);
    pixels.resize(static_cast<size_t>(width) * height * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    size_t brightPixels = 0;
    for (size_t offset = 0; offset < pixels.size(); offset += 4) {
        if (pixels[offset] > 120 && pixels[offset + 1] > 120 && pixels[offset + 2] > 120) ++brightPixels;
    }
    if (glGetError() != GL_NO_ERROR || brightPixels < 100) return false;
    if (path.empty()) return true;
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

bool CheckRenderer(GLExampleRenderer& renderer, int width, int height, void* headingFont) {
    LAMBUI_LOGT(TAG, "CheckRenderer({}x{})", width, height);
    renderer.SetViewportSize(64, 64, width, height);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    UIRenderCommand outer, inner, fill, pop, marker;
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
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    const auto readPixels = [&]() {
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    };
    const auto matches = [&](int logicalX, int logicalY, int red, int green, int blue) {
        const auto pixel = (static_cast<size_t>(height - 1 - logicalY * height / 64) * width + logicalX * width / 64) * 4;
        return std::abs(static_cast<int>(pixels[pixel]) - red) <= 2 &&
               std::abs(static_cast<int>(pixels[pixel + 1]) - green) <= 2 &&
               std::abs(static_cast<int>(pixels[pixel + 2]) - blue) <= 2;
    };
    renderer.SubmitRenderCommands({outer, inner, fill, pop, marker, pop});
    readPixels();
    bool passed = matches(32, 32, 224, 32, 32) && matches(10, 10, 32, 224, 32) &&
                  matches(4, 10, 0, 0, 0) && matches(60, 32, 0, 0, 0);
    for (int direction = 0; direction < 5; ++direction) {
        glClear(GL_COLOR_BUFFER_BIT);
        inner.x = direction == 0 ? 56.0f : direction == 1 ? -16.0f : 24.0f;
        inner.y = direction == 2 ? 56.0f : direction == 3 ? -16.0f : 24.0f;
        inner.width = direction == 4 ? 0.0f : 8.0f;
        inner.height = 8.0f;
        renderer.SubmitRenderCommands({outer, inner, fill, pop, marker, pop});
        readPixels();
        passed = matches(32, 32, 0, 0, 0) && matches(10, 10, 32, 224, 32) && passed;
    }
    LAMBUI_LOGI(TAG, "Scaled/nested/empty clips: {}", passed ? "PASS" : "FAIL");
    renderer.SetViewportSize(width, height);
    UIRenderCommand text;
    text.type = RenderCommandType::DrawString;
    text.text = "Material / Contour i 0123";
    text.x = text.y = 8.0f;
    const auto captureText = [&](void* font) {
        text.fontHandle = font;
        glClear(GL_COLOR_BUFFER_BIT);
        renderer.SubmitRenderCommands({text});
        readPixels();
        return pixels;
    };
    const auto body = captureText(nullptr);
    const auto heading = captureText(headingFont);
    const auto fallback = captureText(&text);
    size_t ink = 0;
    for (size_t pixel = 0; pixel < body.size(); pixel += 4) {
        if (body[pixel] > 64) ++ink;
    }
    const bool fontsPassed = ink > 100 && body != heading && body == fallback;
    LAMBUI_LOGI(TAG, "Font pixels and fallback: {}", fontsPassed ? "PASS" : "FAIL");
    return fontsPassed && glGetError() == GL_NO_ERROR && passed;
}
} // namespace

int main(int argc, char** argv) {
    LambUI::Log::UseDefaultConsoleSink();
    Log::SetMinLevel(LogLevel::Info);
    LAMBUI_LOGI(TAG, "Starting OpenGL 1.1 widget showcase");
    bool smoke = false;
    std::string fontPath, headingFontPath, screenshotPrefix;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--smoke-test") smoke = true;
        else if (argument == "--font" && index + 1 < argc) fontPath = argv[++index];
        else if (argument == "--heading-font" && index + 1 < argc) headingFontPath = argv[++index];
        else if (argument == "--screenshot" && index + 1 < argc) screenshotPrefix = argv[++index];
        else {
            LAMBUI_LOGE(TAG, "Usage: lambui_example_opengl [--font path] [--heading-font path] [--smoke-test] [--screenshot prefix]");
            return 1;
        }
    }
    if (fontPath.empty()) {
        for (const char* candidate : {"C:/Windows/Fonts/segoeui.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                     "/System/Library/Fonts/Supplemental/Arial.ttf"}) {
            if (std::ifstream(candidate).good()) { fontPath = candidate; break; }
        }
    }
    if (headingFontPath.empty()) {
        headingFontPath = fontPath;
        for (const char* candidate : {"C:/Windows/Fonts/georgia.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
                                     "/System/Library/Fonts/Supplemental/Georgia.ttf"}) {
            if (std::ifstream(candidate).good()) { headingFontPath = candidate; break; }
        }
    }

    if (!glfwInit()) {
        LAMBUI_LOGE(TAG, "Failed to initialize GLFW");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    // Leave GLFW_OPENGL_PROFILE at its default (GLFW_OPENGL_ANY_PROFILE): this example
    // relies on legacy immediate-mode GL (glBegin/glVertex), and requesting a specific
    // profile is only valid for context version 3.2+, which would break window creation
    // at the default context version 1.0.

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    if (smoke) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(920, 680, "LambUI - OpenGL 1.1 Asset Browser", nullptr, nullptr);
    if (!window) {
        LAMBUI_LOGE(TAG, "Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(smoke ? 0 : 1);
    glfwSetWindowSizeLimits(window, 360, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
    LAMBUI_LOGI(TAG, "GL {} / {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    const auto destroyWindow = [](GLFWwindow* ownedWindow) {
        LAMBUI_LOGT(TAG, "Destroy window and context");
        glfwDestroyWindow(ownedWindow);
        glfwTerminate();
    };
    const std::unique_ptr<GLFWwindow, decltype(destroyWindow)> windowOwner(window, destroyWindow);
    auto renderer = std::make_shared<GLExampleRenderer>();
    FontAtlas fontAtlas, headingAtlas;
    if (!fontAtlas.LoadFromFile(fontPath, 20) || !renderer->LoadFont(fontAtlas)) {
        LAMBUI_LOGE(TAG, "Cannot load font '{}'; pass --font <path-to-ttf>", fontPath);
        return 1;
    }
    auto textMeasurer = std::make_shared<FontAtlasTextMeasurer>(fontAtlas);
    if (!headingAtlas.LoadFromFile(headingFontPath, 26) || !renderer->LoadFont(headingAtlas, &headingAtlas) ||
        !textMeasurer->RegisterFont(&headingAtlas, headingAtlas)) {
        LAMBUI_LOGE(TAG, "Cannot register heading font '{}'", headingFontPath);
        return 1;
    }

    UIManager uiManager(renderer, textMeasurer);
    uiManager.SetClipboardCallbacks([window](std::string& text) {
        LAMBUI_LOGT(TAG, "ReadClipboard");
        const char* value = glfwGetClipboardString(window);
        if (!value) return false;
        text = value;
        return true;
    }, [window](const std::string& text) {
        LAMBUI_LOGT(TAG, "WriteClipboard(bytes={})", text.size());
        glfwGetError(nullptr);
        glfwSetClipboardString(window, text.c_str());
        return glfwGetError(nullptr) == GLFW_NO_ERROR;
    });
    g_uiManager = &uiManager;

    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCharCallback(window, CharCallback);
    glfwSetKeyCallback(window, [](GLFWwindow* source, int key, int, int action, int) {
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
        else if (key == GLFW_KEY_C) scanCode = ScanCode::C;
        else if (key == GLFW_KEY_X) scanCode = ScanCode::X;
        else if (key == GLFW_KEY_V) scanCode = ScanCode::V;
        else if (key == GLFW_KEY_LEFT) scanCode = ScanCode::Left;
        else if (key == GLFW_KEY_RIGHT) scanCode = ScanCode::Right;
        else if (key == GLFW_KEY_UP) scanCode = ScanCode::Up;
        else if (key == GLFW_KEY_DOWN) scanCode = ScanCode::Down;
        else if (key == GLFW_KEY_HOME) scanCode = ScanCode::Home;
        else if (key == GLFW_KEY_END) scanCode = ScanCode::End;
        else if (key == GLFW_KEY_DELETE) scanCode = ScanCode::Delete;
        if (!g_uiManager) return;
        const bool hadPopup = g_uiManager->GetActivePopup() != nullptr;
        if (scanCode) g_uiManager->InjectKeyEvent(scanCode, action != GLFW_RELEASE);
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !hadPopup) glfwSetWindowShouldClose(source, GLFW_TRUE);
    });
    glfwSetScrollCallback(window, [](GLFWwindow*, double xOffset, double yOffset) {
        LAMBUI_LOGT(TAG, "Scroll({}, {})", xOffset, yOffset);
        if (g_uiManager) {
            g_uiManager->InjectMouseWheel(static_cast<float>(xOffset) * 20.0f,
                                         static_cast<float>(yOffset) * 20.0f);
        }
    });

    UIWidget& root = uiManager.GetRoot();
    auto* title = root.CreateChild<UITextWidget>("Title");
    title->SetMouseEnabled(false);
    title->SetTextMeasurer(uiManager.GetTextMeasurer());
    title->SetFont(&headingAtlas);
    title->SetText("LambUI / OpenGL 1.1");
    title->SetPoint(AnchorPoint::TopLeft, &root, AnchorPoint::TopLeft, 24.0f, 20.0f);
    title->SetColor(0xB9E5D8FFu);
    LambUIExamples::WidgetShowcase widgets(uiManager, [](const UICustomRenderArgs& args) {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glColor4f(18.0f / 255.0f, 44.0f / 255.0f, 40.0f / 255.0f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(args.viewportX, args.viewportY);
        glVertex2f(args.viewportX + args.viewportWidth, args.viewportY);
        glVertex2f(args.viewportX + args.viewportWidth, args.viewportY + args.viewportHeight);
        glVertex2f(args.viewportX, args.viewportY + args.viewportHeight);
        glEnd();
        glColor4f(103.0f / 255.0f, 219.0f / 255.0f, 179.0f / 255.0f, 1.0f);
        for (int band = 0; band < 14; ++band) {
            glBegin(GL_LINE_STRIP);
            for (int sample = 0; sample <= 64; ++sample) {
                const float phase = static_cast<float>(sample) / 64.0f;
                glVertex2f(args.viewportX + phase * args.viewportWidth,
                    args.viewportY + args.viewportHeight * (0.08f + band * 0.06f +
                        0.04f * std::sin(phase * 12.0f + band * 0.6f)));
            }
            glEnd();
        }
    });
    bool passed = true, screenshotSaved = false;
    int smokeFrame = 0, previousWidth = -1, previousHeight = -1;
    double previousTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        if (smoke) glfwSetWindowSize(window, smokeFrame == 0 ? 920 : 420, 680);
        glfwPollEvents();
        int width = 0, height = 0, framebufferWidth = 0, framebufferHeight = 0;
        glfwGetWindowSize(window, &width, &height);
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (width <= 0 || height <= 0 || framebufferWidth <= 0 || framebufferHeight <= 0) {
            if (smoke) { passed = false; break; }
            glfwWaitEvents();
            previousTime = glfwGetTime();
            continue;
        }
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        if (smoke) passed = CheckRenderer(*renderer, framebufferWidth, framebufferHeight, &headingAtlas) && passed;
        renderer->SetViewportSize(width, height, framebufferWidth, framebufferHeight);
        if (width != previousWidth || height != previousHeight) {
            LAMBUI_LOGT(TAG, "Resize({}, {})", width, height);
            uiManager.SetDisplaySize(static_cast<float>(width), static_cast<float>(height));
            widgets.Layout(24.0f, 64.0f, static_cast<float>(width) - 48.0f, static_cast<float>(height) - 88.0f);
            previousWidth = width;
            previousHeight = height;
        }
        const double now = glfwGetTime();
        const float deltaTime = static_cast<float>((std::min)(now - previousTime, 0.1));
        previousTime = now;
        widgets.Update(deltaTime);
        uiManager.Update(deltaTime);
        if (smoke) passed = widgets.SmokeTest() && passed;
        glClearColor(20.0f / 255.0f, 27.0f / 255.0f, 31.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        uiManager.Render();
        const std::string suffix = width < 600 ? "-compact.ppm" : "-desktop.ppm";
        std::vector<unsigned char> basePixels;
        if (smoke || (!screenshotPrefix.empty() && !screenshotSaved)) {
            passed = CaptureFrame(screenshotPrefix.empty() ? "" : screenshotPrefix + suffix,
                                  framebufferWidth, framebufferHeight, basePixels) && passed;
            screenshotSaved = true;
        }
        if (smoke) {
            passed = widgets.CaptureViews([&](const char* state) {
                glClear(GL_COLOR_BUFFER_BIT);
                uiManager.Render();
                std::vector<unsigned char> statePixels;
                const bool valid = CaptureFrame(screenshotPrefix.empty() ? "" : screenshotPrefix + "-" + state + suffix,
                                                framebufferWidth, framebufferHeight, statePixels);
                return valid && statePixels != basePixels;
            }) && passed;
            LAMBUI_LOGI(TAG, "Showcase smoke {}x{}: {}", width, height, passed ? "PASS" : "FAIL");
        }
        glfwSwapBuffers(window);
        if (smoke && ++smokeFrame == 2) break;
    }

    g_uiManager = nullptr;
    return passed ? 0 : 1;
}
