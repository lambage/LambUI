#include "GL33ExampleRenderer.h"
#include "../../common/WidgetShowcase.h"
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
    manager.SetClipboardCallbacks([window](std::string& text) {
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
        const bool hadPopup = Manager(source)->GetActivePopup() != nullptr;
        if (scanCode) Manager(source)->InjectKeyEvent(scanCode, action != GLFW_RELEASE);
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !hadPopup) glfwSetWindowShouldClose(source, GLFW_TRUE);
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

bool CheckTextCoverage(GL33ExampleRenderer& renderer) {
    LAMBUI_LOGT(TAG, "CheckTextCoverage");
    constexpr int width = 384, height = 96, referenceScale = 4;
    GLuint framebuffer = 0, texture = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width * referenceScale, height * referenceScale,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    bool passed = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (passed) {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        UIRenderCommand text;
        text.type = RenderCommandType::DrawString;
        text.text = "Materials / SDF 0123";
        std::vector<unsigned char> pixels(width * height * 4);
        std::vector<unsigned char> reference(width * height * referenceScale * referenceScale * 4);
        text.text = "i";
        double minimumInk = 1.0e30, maximumInk = 0.0;
        std::array<double, 8> phaseInk{};
        bool translationPassed = true;
        for (int phase = 0; phase < 16; ++phase) {
            text.x = 8.0f + phase / 8.0f;
            text.y = 8.0f;
            renderer.SetViewportSize(width, height, width, height);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.SubmitRenderCommands({text});
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            double ink = 0.0;
            int peak = 0;
            for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                ink += pixels[pixel] / 255.0;
                peak = std::max(peak, static_cast<int>(pixels[pixel]));
            }
            minimumInk = std::min(minimumInk, ink);
            maximumInk = std::max(maximumInk, ink);
            if (phase < 8) phaseInk[phase] = ink;
            else translationPassed = std::abs(ink - phaseInk[phase - 8]) < 0.02 * ink && translationPassed;
            LAMBUI_LOGI(TAG, "Thin glyph i phase={}: ink={:.3f}, peak={}", phase, ink, peak);
        }
        const bool thinPassed = maximumInk > 0.0 && minimumInk / maximumInk > 0.85;
        LAMBUI_LOGI(TAG, "Thin glyph phase stability: {} (ratio={:.3f})",
                    thinPassed ? "PASS" : "FAIL", maximumInk > 0.0 ? minimumInk / maximumInk : 0.0);
        LAMBUI_LOGI(TAG, "Thin glyph integer translation: {}", translationPassed ? "PASS" : "FAIL");
        passed = thinPassed && translationPassed && passed;
        for (const char* label : {"i", "Material / Contour"}) {
            text.text = label;
            for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
                const int logicalWidth = static_cast<int>(width / scale);
                const int logicalHeight = static_cast<int>(height / scale);
                const float pixelWidth = static_cast<float>(logicalWidth) / width;
                const float pixelHeight = static_cast<float>(logicalHeight) / height;
                renderer.SetViewportSize(logicalWidth, logicalHeight, width, height);
                double worstError = 0.0;
                for (const float phase : {0.0f, 0.25f, 0.5f, 0.75f}) {
                    text.x = (8.0f + phase) * pixelWidth;
                    text.y = (8.0f + phase) * pixelHeight;
                    glClear(GL_COLOR_BUFFER_BIT);
                    renderer.SubmitRenderCommands({text});
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    for (int shift = 1; shift <= 3; ++shift) {
                        const int shiftX = shift & 1;
                        const int shiftY = (shift >> 1) & 1;
                        text.x = (8.0f + phase + shiftX) * pixelWidth;
                        text.y = (8.0f + phase + shiftY) * pixelHeight;
                        glClear(GL_COLOR_BUFFER_BIT);
                        renderer.SubmitRenderCommands({text});
                        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
                        double difference = 0.0, ink = 0.0;
                        for (int row = 1; row < height; ++row) {
                            for (int column = 0; column < width - 1; ++column) {
                                const auto original = pixels[(row * width + column) * 4];
                                const auto moved = reference[((row - shiftY) * width + column + shiftX) * 4];
                                ink += original;
                                difference += std::abs(static_cast<int>(original) - moved);
                            }
                        }
                        worstError = std::max(worstError, ink > 0.0 ? difference / ink : 1.0);
                    }
                }
                const bool movementPassed = worstError < 0.02;
                passed = movementPassed && passed;
                LAMBUI_LOGI(TAG, "Text translation '{}' scale={}: {} (worst error={:.4f})",
                            label, scale, movementPassed ? "PASS" : "FAIL", worstError);
            }
        }
        text.text = "Materials / SDF 0123";
        for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
            for (const float offset : {0.0f, 0.25f, 0.5f, 0.75f}) {
                text.x = text.y = 8.0f + offset;
                renderer.SetViewportSize(static_cast<int>(width / scale), static_cast<int>(height / scale), width, height);
                glClear(GL_COLOR_BUFFER_BIT);
                renderer.SubmitRenderCommands({text});
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                renderer.SetViewportSize(static_cast<int>(width / scale), static_cast<int>(height / scale),
                                         width * referenceScale, height * referenceScale);
                glClear(GL_COLOR_BUFFER_BIT);
                renderer.SubmitRenderCommands({text});
                glReadPixels(0, 0, width * referenceScale, height * referenceScale,
                             GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
                double difference = 0.0, referenceInk = 0.0;
                for (int row = 0; row < height; ++row) {
                    for (int column = 0; column < width; ++column) {
                        double coverage = 0.0;
                        for (int sampleY = 0; sampleY < referenceScale; ++sampleY) {
                            for (int sampleX = 0; sampleX < referenceScale; ++sampleX) {
                                const auto sample = ((row * referenceScale + sampleY) * width * referenceScale
                                                   + column * referenceScale + sampleX) * 4;
                                coverage += reference[sample];
                            }
                        }
                        coverage /= referenceScale * referenceScale;
                        referenceInk += coverage;
                        difference += std::abs(pixels[(row * width + column) * 4] - coverage);
                    }
                }
                const double error = referenceInk > 0.0 ? difference / referenceInk : 1.0;
                const bool coveragePassed = error < 0.15;
                passed = coveragePassed && passed;
                LAMBUI_LOGI(TAG, "Text coverage scale={} offset={}: {} (relative error={:.3f})",
                            scale, offset, coveragePassed ? "PASS" : "FAIL", error);
            }
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
    return glGetError() == GL_NO_ERROR && passed;
}

bool CheckTextureAtlas(GL33ExampleRenderer& renderer, int width, int height) {
    LAMBUI_LOGT(TAG, "CheckTextureAtlas");
    renderer.SetViewportSize(width, height, width, height);
    TextureAtlas atlas(8, 4);
    const auto red = atlas.AddImage(1, 1, {255, 0, 0, 255});
    const auto blue = atlas.AddImage(1, 1, {0, 0, 255, 255});
    if (!red || !blue) return false;
    const auto uploads = renderer.GetTextureUploadCount();
    void* handle = renderer.UploadTextureAtlas(atlas);
    bool passed = handle && renderer.GetTextureUploadCount() == uploads + 1;
    passed = renderer.UploadTextureAtlas(atlas) == handle &&
             renderer.GetTextureUploadCount() == uploads + 1 && passed;
    std::vector<UIRenderCommand> commands;
    for (const auto& region : {*red, *blue}) {
        UIRenderCommand quad;
        quad.x = static_cast<float>(commands.size() * 32);
        quad.width = quad.height = 32;
        quad.textureHandle = handle;
        quad.u0 = region.u0; quad.v0 = region.v0;
        quad.u1 = region.u1; quad.v1 = region.v1;
        commands.push_back(quad);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands(commands);
    const auto pixels = ReadPixels(width, height);
    const auto pixelMatches = [&](const std::vector<unsigned char>& image, int column, int redValue, int greenValue, int blueValue) {
        const size_t offset = (static_cast<size_t>(height - 16) * width + column) * 4;
        return image[offset] == redValue && image[offset + 1] == greenValue && image[offset + 2] == blueValue;
    };
    passed = renderer.GetDrawCallCount() == 1 && pixelMatches(pixels, 0, 255, 0, 0) &&
             pixelMatches(pixels, 31, 255, 0, 0) && pixelMatches(pixels, 32, 0, 0, 255) && passed;
    passed = atlas.UpdateImage(red->id, {0, 255, 0, 255}) && passed;
    passed = renderer.UploadTextureAtlas(atlas) == handle &&
             renderer.GetTextureUploadCount() == uploads + 2 && passed;
    renderer.SubmitRenderCommands(commands);
    const auto updated = ReadPixels(width, height);
    passed = renderer.GetUploadCount() == 0 && pixelMatches(updated, 0, 0, 255, 0) &&
             pixelMatches(updated, 32, 0, 0, 255) && passed;
    renderer.ReleaseTextureAtlas(atlas);
    renderer.ReleaseTextureAtlas(atlas);
    passed = glGetError() == GL_NO_ERROR && passed;
    LAMBUI_LOGI(TAG, "RGBA atlas shared batch/gutters/revision uploads: {}", passed ? "PASS" : "FAIL");
    return passed;
}

bool CheckBatching(GL33ExampleRenderer& renderer, int width, int height) {
    LAMBUI_LOGT(TAG, "CheckBatching");
    renderer.SetViewportSize(width, height, width, height);
    UIRenderCommand quad;
    quad.width = quad.height = 32;
    std::vector<UIRenderCommand> commands(100, quad);
    for (size_t index = 0; index < commands.size(); ++index) {
        commands[index].color = index % 2 ? 0xFF000080u : 0x00FF0080u;
        commands[index].x = static_cast<float>(index);
    }
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands(commands);
    bool passed = renderer.GetDrawCallCount() == 1 && renderer.GetQuadCount() == 100 &&
                  renderer.GetUploadCount() == 1;
    const auto batched = ReadPixels(width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands(commands);
    passed = renderer.GetUploadCount() == 0 && batched == ReadPixels(width, height) && passed;
    LAMBUI_LOGI(TAG, "100 translucent quads: {} draw call, unchanged frame: {} buffer uploads",
                renderer.GetDrawCallCount(), renderer.GetUploadCount());
    glClear(GL_COLOR_BUFFER_BIT);
    for (const auto& command : commands) renderer.SubmitRenderCommands({command});
    passed = batched == ReadPixels(width, height) && passed;
    renderer.SubmitRenderCommands(commands);
    commands.back().color = 0x0000FFFFu;
    renderer.SubmitRenderCommands(commands);
    passed = renderer.GetUploadCount() == 1 && passed;
    renderer.SubmitRenderCommands(commands);
    passed = renderer.GetUploadCount() == 0 && passed;

    UIRenderCommand text;
    text.type = RenderCommandType::DrawString;
    text.text = "Batch glyphs";
    renderer.SubmitRenderCommands({text});
    passed = renderer.GetDrawCallCount() == 1 && renderer.GetQuadCount() > 5 && passed;
    renderer.SubmitRenderCommands({text});
    passed = renderer.GetUploadCount() == 0 && passed;
    text.text += "!";
    renderer.SubmitRenderCommands({text});
    passed = renderer.GetUploadCount() == 1 && passed;

    int callbacks = 0;
    UIRenderCommand callback;
    callback.type = RenderCommandType::CustomCallback;
    callback.customRenderFunc = [&](const UICustomRenderArgs&) {
        ++callbacks;
        glUseProgram(0);
        glBindVertexArray(0);
    };
    renderer.SubmitRenderCommands({quad, callback, quad});
    renderer.SubmitRenderCommands({quad, callback, quad});
    passed = callbacks == 2 && renderer.GetDrawCallCount() == 2 && renderer.GetUploadCount() == 0 && passed;
    UIRenderCommand changed = quad;
    changed.x = 40;
    renderer.SubmitRenderCommands({quad, callback, changed});
    passed = renderer.GetUploadCount() == 1 && renderer.GetDrawCallCount() == 2 && passed;
    renderer.SetViewportSize(width / 2, height / 2, width, height);
    renderer.SubmitRenderCommands({quad, callback, changed});
    passed = renderer.GetUploadCount() == 0 && passed;
    renderer.SetViewportSize(width, height, width, height);
    commands.assign(4097, quad);
    renderer.SubmitRenderCommands(commands);
    passed = renderer.GetQuadCount() == 4097 && renderer.GetDrawCallCount() == 2 && passed;
    renderer.SubmitRenderCommands(commands);
    passed = renderer.GetUploadCount() == 0 && passed;
    renderer.SubmitRenderCommands({});
    passed = renderer.GetDrawCallCount() == 0 && renderer.GetUploadCount() == 0 && passed;
    renderer.SubmitRenderCommands({quad});
    passed = renderer.GetUploadCount() == 1 && glGetError() == GL_NO_ERROR && passed;
    LAMBUI_LOGI(TAG, "Batching/order/dirty uploads/callbacks/capacity: {}", passed ? "PASS" : "FAIL");
    return passed;
}

bool CheckRenderer(GL33ExampleRenderer& renderer, int width, int height, void* headingFont) {
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
    for (const float displayScale : {1.0f, 1.5f, 2.0f}) {
        renderer.SetViewportSize(static_cast<int>(width / displayScale), static_cast<int>(height / displayScale), width, height);
        for (const float offset : {0.0f, 0.5f}) {
            glClear(GL_COLOR_BUFFER_BIT);
            text.text = "Materials / SDF 0123";
            text.x = 8.0f + offset;
            text.y = 8.0f + offset;
            renderer.SubmitRenderCommands({text});
            const auto textPixels = ReadPixels(width, height);
            size_t visiblePixels = 0, softPixels = 0, solidPixels = 0;
            for (size_t pixel = 0; pixel < textPixels.size(); pixel += 4) {
                const auto coverage = textPixels[pixel];
                if (coverage > 0) ++visiblePixels;
                if (coverage > 16 && coverage < 240) ++softPixels;
                if (coverage >= 240) ++solidPixels;
            }
            const bool textPassed = visiblePixels > 100 && softPixels > visiblePixels / 5 && solidPixels > 5;
            passed = textPassed && passed;
            LAMBUI_LOGI(TAG, "UI text AA scale={} offset={}: {} (ink={}, soft={}, solid={})",
                        displayScale, offset, textPassed ? "PASS" : "FAIL", visiblePixels, softPixels, solidPixels);
        }
    }
    renderer.SetViewportSize(width, height, width, height);
    text.text = "LambUI 0123";
    text.x = text.y = 8.0f;
    text.fontHandle = nullptr;
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands({text});
    const auto defaultPixels = ReadPixels(width, height);
    text.fontHandle = headingFont;
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands({text});
    const auto headingPixels = ReadPixels(width, height);
    size_t headingInk = 0;
    for (size_t pixel = 0; pixel < headingPixels.size(); pixel += 4) {
        if (headingPixels[pixel] > 0) ++headingInk;
    }
    text.fontHandle = &text;
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.SubmitRenderCommands({text});
    const bool fontsPassed = headingInk > 20 && defaultPixels != headingPixels &&
                             defaultPixels == ReadPixels(width, height);
    LAMBUI_LOGI(TAG, "Multiple-font pixels and fallback: {}", fontsPassed ? "PASS" : "FAIL");
    passed = fontsPassed && glGetError() == GL_NO_ERROR && passed;
    passed = CheckTextCoverage(renderer) && passed;
    passed = CheckBatching(renderer, width, height) && passed;
    passed = CheckTextureAtlas(renderer, width, height) && passed;
    renderer.SetViewportSize(width, height, width, height);
    return passed;
}

int RunExample(GLFWwindow* window, const std::string& fontPath, const std::string& headingFontPath,
               bool smoke, const std::string& screenshotPrefix) {
    LAMBUI_LOGT(TAG, "RunExample(smoke={})", smoke);
    auto renderer = std::make_shared<GL33ExampleRenderer>();
    if (!renderer->Initialize()) return 1;
    FontAtlas atlas;
    if (!atlas.LoadFromFile(fontPath, 20) || !renderer->LoadFont(atlas)) {
        LAMBUI_LOGE(TAG, "Cannot load font '{}'; pass --font <path-to-ttf>", fontPath);
        return 1;
    }
    auto measurer = std::make_shared<FontAtlasTextMeasurer>(atlas);
    FontAtlas headingAtlas;
    if (!headingAtlas.LoadFromFile(headingFontPath, 26) || !renderer->LoadFont(headingAtlas, &headingAtlas) ||
        !measurer->RegisterFont(&headingAtlas, headingAtlas)) {
        LAMBUI_LOGE(TAG, "Cannot register heading font '{}'", headingFontPath);
        return 1;
    }
    if (smoke) {
        float bodyWidth = 0, bodyHeight = 0, headingWidth = 0, headingHeight = 0;
        measurer->MeasureText("MMMM", nullptr, bodyWidth, bodyHeight);
        measurer->MeasureText("MMMM", &headingAtlas, headingWidth, headingHeight);
        const auto* glyph = headingAtlas.FindGlyph(U'M');
        const bool metricsPassed = glyph && std::abs(headingWidth - glyph->advance * 4) < 0.01f &&
            headingHeight == headingAtlas.GetLineHeight() && bodyHeight != headingHeight && bodyWidth != headingWidth;
        LAMBUI_LOGI(TAG, "Multiple-font atlas metrics: {}", metricsPassed ? "PASS" : "FAIL");
        if (!metricsPassed) return 1;
    }
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
    label(*header, "Title", "LambUI / OpenGL 3.3", 24.0f, 18.0f, 0xF0F4F3FFu)->SetFont(&headingAtlas);
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
    button->SetTooltip("Pause the contour animation");
    slider->SetTooltip("Contour distortion");
    LambUIExamples::WidgetShowcase widgets(manager, [&renderer](const UICustomRenderArgs& args) { renderer->DrawEffect(args); });

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
        if (smoke) passed = CheckRenderer(*renderer, framebufferWidth, framebufferHeight, &headingAtlas) && passed;
        renderer->SetViewportSize(width, height, framebufferWidth, framebufferHeight);
        if (width != previousWidth || height != previousHeight) {
            float contentScaleX = 1.0f, contentScaleY = 1.0f;
            glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);
            LAMBUI_LOGI(TAG, "Display: window={}x{}, framebuffer={}x{}, content scale={}x{}",
                        width, height, framebufferWidth, framebufferHeight, contentScaleX, contentScaleY);
            manager.SetDisplaySize(static_cast<float>(width), static_cast<float>(height));
            const float panelWidth = width < 700 ? static_cast<float>(width) - 32.0f : 320.0f;
            widgets.Layout(static_cast<float>(width) - panelWidth - 16.0f, 124.0f,
                           panelWidth, std::min(380.0f, static_cast<float>(height) - 320.0f));
            previousWidth = width;
            previousHeight = height;
        }
        const double now = glfwGetTime();
        const float deltaTime = static_cast<float>(std::min(now - previousTime, 0.1));
        previousTime = now;
        if (!paused) effectTime += deltaTime;
        widgets.Update(deltaTime);
        manager.Update(deltaTime);
        if (smoke) {
            passed = widgets.SmokeTest() && passed;
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
            if (smoke) {
                const auto basePixels = ReadPixels(framebufferWidth, framebufferHeight);
                passed = widgets.CaptureViews([&](const char* state) {
                    glClear(GL_COLOR_BUFFER_BIT);
                    manager.Render();
                    const bool changed = ReadPixels(framebufferWidth, framebufferHeight) != basePixels;
                    return SaveScreenshot(screenshotPrefix + "-" + state + suffix, framebufferWidth, framebufferHeight) && changed;
                }) && passed;
            }
            screenshotSaved = true;
        }
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
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
    std::string headingFontPath;
    std::string screenshotPrefix;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--smoke-test") smoke = true;
        else if (argument == "--font" && index + 1 < argc) fontPath = argv[++index];
        else if (argument == "--heading-font" && index + 1 < argc) headingFontPath = argv[++index];
        else if (argument == "--screenshot" && index + 1 < argc) screenshotPrefix = argv[++index];
        else {
            LAMBUI_LOGE(TAG, "Usage: lambui_example_opengl33 [--font path] [--heading-font path] [--smoke-test] [--screenshot prefix]");
            return 1;
        }
    }
    if (fontPath.empty()) {
        for (const char* candidate : {"C:/Windows/Fonts/segoeui.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                     "/System/Library/Fonts/Supplemental/Arial.ttf"}) {
            if (std::ifstream(candidate).good()) {
                fontPath = candidate;
                break;
            }
        }
    }
    if (headingFontPath.empty()) {
        headingFontPath = fontPath;
        for (const char* candidate : {"C:/Windows/Fonts/georgia.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
                                     "/System/Library/Fonts/Supplemental/Georgia.ttf"}) {
            if (std::ifstream(candidate).good()) {
                headingFontPath = candidate;
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
    glfwSetWindowSizeLimits(window, 360, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSwapInterval(smoke ? 0 : 1);
    const int result = RunExample(window, fontPath, headingFontPath, smoke, screenshotPrefix);
    LAMBUI_LOGT(TAG, "Destroy window and terminate GLFW");
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}