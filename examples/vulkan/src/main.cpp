#include "VulkanExampleRenderer.h"
#include "LambUI/LambUI.h"
#include "LambUILua/LuaBindings.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "../../common/src/GlfwPointer.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace LambUI;

namespace {
constexpr const char* TAG = "VulkanApplication";

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void Execute(lua_State* lua, const std::string& code) {
    LAMBUI_LOGT(TAG, "Execute Lua host command");
    const int top = lua_gettop(lua);
    const int result = luaL_dostring(lua, code.c_str());
    const std::string error = result == LUA_OK ? "" : (lua_tostring(lua, -1) ? lua_tostring(lua, -1) : "non-string Lua error");
    lua_settop(lua, top);
    Require(result == LUA_OK, error);
}

void Call(lua_State* lua, const char* method, const std::vector<double>& arguments) {
    const int top = lua_gettop(lua);
    lua_getglobal(lua, "App");
    lua_getfield(lua, -1, method);
    for (auto value : arguments) lua_pushnumber(lua, value);
    const int result = lua_pcall(lua, static_cast<int>(arguments.size()), 0, 0);
    const std::string error = result == LUA_OK ? "" : (lua_tostring(lua, -1) ? lua_tostring(lua, -1) : "non-string Lua error");
    lua_settop(lua, top);
    Require(result == LUA_OK, std::string("App.") + method + ": " + error);
}

int DrawField(lua_State* lua) {
    const auto time = luaL_checknumber(lua, 1);
    const auto strength = luaL_checknumber(lua, 2);
    if (!std::isfinite(time) || !std::isfinite(strength) || std::abs(time) > 1.0e6)
        return luaL_error(lua, "Field parameters must be finite and time within +/- 1000000 seconds");
    auto* renderer = static_cast<VulkanExampleRenderer*>(lua_touserdata(lua, lua_upvalueindex(1)));
    try {
        renderer->DrawField(static_cast<float>(time), static_cast<float>(strength));
        return 0;
    } catch (const std::exception& error) {
        lua_pushstring(lua, error.what());
    }
    return lua_error(lua);
}

UIManager* Manager(GLFWwindow* window) { return static_cast<UIManager*>(glfwGetWindowUserPointer(window)); }

uint32_t MapKey(int key) {
    switch (key) {
        case GLFW_KEY_BACKSPACE: return ScanCode::Backspace;
        case GLFW_KEY_TAB: return ScanCode::Tab;
        case GLFW_KEY_ENTER: case GLFW_KEY_KP_ENTER: return ScanCode::Enter;
        case GLFW_KEY_ESCAPE: return ScanCode::Escape;
        case GLFW_KEY_SPACE: return ScanCode::Space;
        case GLFW_KEY_LEFT: return ScanCode::Left;
        case GLFW_KEY_RIGHT: return ScanCode::Right;
        case GLFW_KEY_UP: return ScanCode::Up;
        case GLFW_KEY_DOWN: return ScanCode::Down;
        case GLFW_KEY_HOME: return ScanCode::Home;
        case GLFW_KEY_END: return ScanCode::End;
        case GLFW_KEY_DELETE: return ScanCode::Delete;
        case GLFW_KEY_LEFT_SHIFT: return ScanCode::LeftShift;
        case GLFW_KEY_RIGHT_SHIFT: return ScanCode::RightShift;
        case GLFW_KEY_LEFT_CONTROL: return ScanCode::LeftControl;
        case GLFW_KEY_RIGHT_CONTROL: return ScanCode::RightControl;
        case GLFW_KEY_A: return ScanCode::A;
        case GLFW_KEY_C: return ScanCode::C;
        case GLFW_KEY_V: return ScanCode::V;
        case GLFW_KEY_X: return ScanCode::X;
        default: return 0;
    }
}

void ConnectInput(GLFWwindow* window, UIManager& manager) {
    LAMBUI_LOGT(TAG, "Connect injected input and clipboard");
    glfwSetWindowUserPointer(window, &manager);
    glfwSetCursorEnterCallback(window, [](GLFWwindow* host, int entered) {
        LAMBUI_LOGT(TAG, "Pointer entered={}", entered);
        if (!Manager(host)) return;
        if (!entered) Manager(host)->InjectMouseLeave();
        else {
            double mouseX = 0, mouseY = 0;
            glfwGetCursorPos(host, &mouseX, &mouseY);
            Manager(host)->InjectMouseMove(float(mouseX), float(mouseY));
        }
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* host, double x, double y) {
        LAMBUI_LOGT(TAG, "Pointer {},{}", x, y);
        Manager(host)->InjectMouseMove(float(x), float(y));
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* host, int button, int action, int) {
        LAMBUI_LOGT(TAG, "Mouse button {}, action {}", button, action);
        if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT && button != GLFW_MOUSE_BUTTON_MIDDLE) return;
        Manager(host)->InjectMouseButton(button == GLFW_MOUSE_BUTTON_LEFT ? MouseButton::Left :
            (button == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right : MouseButton::Middle), action == GLFW_PRESS);
    });
    glfwSetScrollCallback(window, [](GLFWwindow* host, double x, double y) {
        LAMBUI_LOGT(TAG, "Wheel {},{}", x, y);
        Manager(host)->InjectMouseWheel(float(x) * 36, float(y) * 36);
    });
    glfwSetKeyCallback(window, [](GLFWwindow* host, int key, int, int action, int) {
        LAMBUI_LOGT(TAG, "Key {}, action {}", key, action);
        const auto mapped = MapKey(key);
        if (mapped) Manager(host)->InjectKeyEvent(mapped, action != GLFW_RELEASE);
    });
    glfwSetCharCallback(window, [](GLFWwindow* host, unsigned int codepoint) {
        LAMBUI_LOGT(TAG, "Character {}", codepoint);
        Manager(host)->InjectCharacter(static_cast<char32_t>(codepoint));
    });
    manager.SetClipboardCallbacks([window](std::string& text) {
        LAMBUI_LOGT(TAG, "Read clipboard");
        const auto* value = glfwGetClipboardString(window);
        if (!value) return false;
        text = value;
        return true;
    }, [window](const std::string& text) {
        LAMBUI_LOGT(TAG, "Write clipboard");
        glfwGetError(nullptr);
        glfwSetClipboardString(window, text.c_str());
        return glfwGetError(nullptr) == GLFW_NO_ERROR;
    });
}

std::string FindFont(const std::string& overridePath, bool heading) {
    if (!overridePath.empty()) return overridePath;
    const std::vector<std::string> candidates = heading ? std::vector<std::string>{
        "C:/Windows/Fonts/georgia.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", "/System/Library/Fonts/Supplemental/Georgia.ttf"
    } : std::vector<std::string>{
        "C:/Windows/Fonts/candara.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Trebuchet MS.ttf"
    };
    for (const auto& path : candidates) if (std::ifstream(path, std::ios::binary)) return path;
    throw std::runtime_error("No system font found; pass --font and --heading-font paths");
}

std::vector<uint8_t> Terrain(int width, int height) {
    LAMBUI_LOGT(TAG, "Generate expedition terrain bitmap");
    std::vector<uint8_t> pixels(size_t(width) * height * 4);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const float x = float(column) / width, y = float(row) / height;
            const float altitude = 0.5f + 0.18f * std::sin(x * 14 + std::sin(y * 8)) +
                0.15f * std::cos(y * 13 - x * 5) + 0.08f * std::sin(x * 31 + y * 17);
            const float contour = std::abs(std::fmod(altitude * 22, 1.0f) - 0.5f);
            const float shade = contour < 0.065f ? 0.73f : 1.0f;
            const bool water = altitude < 0.25f;
            const size_t offset = (size_t(row) * width + column) * 4;
            pixels[offset] = uint8_t((water ? 92 : 166 + altitude * 48) * shade);
            pixels[offset+1] = uint8_t((water ? 160 : 185 + altitude * 35) * shade);
            pixels[offset+2] = uint8_t((water ? 186 : 154 + altitude * 34) * shade);
            pixels[offset+3] = 255;
        }
    }
    return pixels;
}

void Render(UIManager& manager, VulkanExampleRenderer& renderer, bool capture) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (renderer.BeginFrame()) { manager.Render(); renderer.EndFrame(capture); return; }
        glfwPollEvents();
    }
    throw std::runtime_error("No drawable swapchain after 20 attempts");
}

void CheckRenderer(VulkanExampleRenderer& renderer, int width, int height) {
    LAMBUI_LOGI(TAG, "Checking GPU quads, alpha, UVs, text, scissors, callback ordering");
    const auto texture = renderer.UploadTexture(2, 2, {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255});
    std::vector<UIRenderCommand> commands;
    auto quad = [&](float x, float y, float wide, float high, uint32_t color) {
        UIRenderCommand item;
        item.x = x; item.y = y; item.width = wide; item.height = high; item.color = color;
        commands.push_back(item);
    };
    quad(0, 0, float(width), float(height), 0x000000FF);
    quad(10, 10, 40, 40, 0xFF0000FF);
    quad(10, 10, 40, 40, 0x0000FF80);
    quad(60, 10, 40, 40, 0xFFFFFFFF);
    commands.back().textureHandle = texture;
    commands.back().u0 = commands.back().u1 = 0.25f;
    commands.back().v0 = commands.back().v1 = 0.75f;
    UIRenderCommand push;
    push.type = RenderCommandType::PushScissor;
    push.x = 110; push.y = 10; push.width = 40; push.height = 40;
    commands.push_back(push);
    quad(0, 0, 200, 100, 0x00FF00FF);
    for (const auto& rect : std::vector<UIRect>{{-90,10,20,20}, {width+10.0f,10,20,20},
            {110,-90,20,20}, {110,height+10.0f,20,20}, {120,20,0,10}}) {
        auto empty = push;
        empty.x = rect.x; empty.y = rect.y; empty.width = rect.width; empty.height = rect.height;
        commands.push_back(empty);
        quad(0, 0, float(width), float(height), 0xFF00FFFF);
        UIRenderCommand pop; pop.type = RenderCommandType::PopScissor;
        commands.push_back(pop);
    }
    UIRenderCommand pop; pop.type = RenderCommandType::PopScissor;
    commands.push_back(pop);
    bool callbackRan = false;
    UIRenderCommand callback;
    callback.type = RenderCommandType::CustomCallback;
    callback.customRenderFunc = [&](const UICustomRenderArgs&) {
        LAMBUI_LOGT(TAG, "GPU smoke custom callback");
        callbackRan = true;
        VkViewport viewport{0, 0, 1, 1, 0, 1};
        VkRect2D clip{{0,0}, {0,0}};
        vkCmdSetViewport(renderer.GetCommandBuffer(), 0, 1, &viewport);
        vkCmdSetScissor(renderer.GetCommandBuffer(), 0, 1, &clip);
    };
    commands.push_back(callback);
    quad(160, 10, 40, 40, 0xFFFFFFFF);
    UIRenderCommand text;
    text.type = RenderCommandType::DrawString;
    text.text = "Fieldwork / iii"; text.x = 12; text.y = 70;
    commands.push_back(text);
    Require(renderer.BeginFrame(), "GPU smoke could not begin frame");
    renderer.SubmitRenderCommands(commands);
    renderer.EndFrame(true);
    const auto& pixels = renderer.GetCapture();
    auto pixel = [&](int x, int y, int red, int green, int blue) {
        const size_t offset = (size_t(y * renderer.GetHeight() / height) * renderer.GetWidth() + x * renderer.GetWidth() / width) * 4;
        Require(std::abs(int(pixels[offset])-red) <= 2 && std::abs(int(pixels[offset+1])-green) <= 2 &&
                std::abs(int(pixels[offset+2])-blue) <= 2, "GPU pixel mismatch at " + std::to_string(x) + "," + std::to_string(y));
    };
    pixel(20,20,127,0,128); pixel(70,20,0,0,255); pixel(120,20,0,255,0);
    pixel(105,20,0,0,0); pixel(155,20,0,0,0); pixel(170,20,255,255,255);
    Require(callbackRan, "Custom callback was skipped");
    size_t ink = 0, antialias = 0;
    for (uint32_t row = 70 * renderer.GetHeight() / height; row < 100 * renderer.GetHeight() / height; ++row)
        for (uint32_t column = 10 * renderer.GetWidth() / width; column < 200 * renderer.GetWidth() / width; ++column) {
            const auto red = pixels[(size_t(row) * renderer.GetWidth() + column) * 4];
            if (red > 200) ++ink;
            if (red > 10 && red < 240) ++antialias;
        }
    Require(ink > 30 && antialias > 30, "SDF text is missing or not antialiased");
    const auto reference = pixels;
    commands.back().fontHandle = reinterpret_cast<void*>(uintptr_t{99999});
    Require(renderer.BeginFrame(), "Font fallback frame unavailable");
    renderer.SubmitRenderCommands(commands); renderer.EndFrame(true);
    Require(reference == renderer.GetCapture(), "Unknown font did not fall back to default");
}

void Click(lua_State* lua, UIManager& manager, const std::string& name) {
    LAMBUI_LOGT(TAG, "Smoke click {}", name);
    Execute(lua, "App.viewport:SetScrollOffset(0,0)");
    manager.Update(0);
    Execute(lua, "App.Reveal('" + name + "')");
    manager.Update(0);
    const int top = lua_gettop(lua);
    Require(luaL_dostring(lua, ("return App.targets['" + name + "']:GetRect()").c_str()) == LUA_OK, "Missing smoke target " + name);
    const float x = float(lua_tonumber(lua, top+1)), y = float(lua_tonumber(lua, top+2));
    const float width = float(lua_tonumber(lua, top+3)), height = float(lua_tonumber(lua, top+4));
    lua_settop(lua, top);
    manager.InjectMouseMove(x + std::min(20.0f, width/2), y + height/2);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.Update(0);
}

UIRect LuaRect(lua_State* lua, const std::string& expression) {
    const int top = lua_gettop(lua);
    const int result = luaL_dostring(lua, ("return " + expression + ":GetRect()").c_str());
    Require(result == LUA_OK, "Cannot read rectangle for " + expression);
    UIRect rect{float(lua_tonumber(lua, top+1)), float(lua_tonumber(lua, top+2)),
                float(lua_tonumber(lua, top+3)), float(lua_tonumber(lua, top+4))};
    lua_settop(lua, top);
    return rect;
}

void PointerClick(UIManager& manager, float x, float y) {
    LAMBUI_LOGT(TAG, "Smoke pointer click {},{}", x, y);
    manager.InjectMouseMove(x, y);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.Update(0);
}

void SmokeWindows(lua_State* lua, UIManager& manager, VulkanExampleRenderer& renderer,
                  const std::string& prefix, const std::string& size) {
    LAMBUI_LOGI(TAG, "Checking animated canvas and window z-order at {}", size);
    Execute(lua, "App.ArrangeWindows(); App.notesWindow:Close(); App.fieldPaused = false; App.fieldTime = 0; App.distortion:SetValue(0.8)");
    manager.Update(0);
    Click(lua, manager, "showCanvas");
    Render(manager, renderer, true);
    auto first = renderer.GetCapture();
    const auto canvas = LuaRect(lua, "App.canvas");
    const auto root = LuaRect(lua, "UI.Root");
    auto differences = [&](const std::vector<uint8_t>& before, const std::vector<uint8_t>& after, const UIRect& region) {
        const float scaleX = renderer.GetWidth() / root.width, scaleY = renderer.GetHeight() / root.height;
        const int left = std::max(0, int(std::ceil(region.x * scaleX)));
        const int top = std::max(0, int(std::ceil(region.y * scaleY)));
        const int right = std::min(int(renderer.GetWidth()), int(std::floor((region.x + region.width) * scaleX)));
        const int bottom = std::min(int(renderer.GetHeight()), int(std::floor((region.y + region.height) * scaleY)));
        size_t changed = 0;
        for (int row = top; row < bottom; ++row) for (int column = left; column < right; ++column) {
            const size_t offset = (size_t(row) * renderer.GetWidth() + column) * 4;
            if (before[offset] != after[offset] || before[offset+1] != after[offset+1] || before[offset+2] != after[offset+2]) ++changed;
        }
        return changed;
    };
    Call(lua, "Update", {0.75}); manager.Update(0);
    Render(manager, renderer, true);
    Require(differences(first, renderer.GetCapture(), canvas) > 100, "Canvas shader is blank or not animated");
    Require(differences(first, renderer.GetCapture(), root) == differences(first, renderer.GetCapture(), canvas), "Canvas escaped its bounds");
    Click(lua, manager, "pauseField");
    Execute(lua, "assert(App.fieldPaused)");
    Render(manager, renderer, true); first = renderer.GetCapture();
    Call(lua, "Update", {1.0}); manager.Update(0);
    Render(manager, renderer, true);
    Require(first == renderer.GetCapture(), "Paused field changed pixels");
    Click(lua, manager, "distortion");
    const auto slider = LuaRect(lua, "App.distortion");
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(slider.x + slider.width * 0.75f, slider.y + slider.height / 2);
    manager.InjectMouseButton(MouseButton::Left, false); manager.Update(0);
    Execute(lua, "assert(math.abs(App.distortion:GetValue() - 1.5) < 0.01)");
    Render(manager, renderer, true);
    Require(differences(first, renderer.GetCapture(), canvas) > 100, "Distortion control did not change canvas");
    auto previewOnly = renderer.GetCapture();
    Click(lua, manager, "showNotes");
    Render(manager, renderer, true);
    const auto notesFront = renderer.GetCapture();
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-windows-" + size + ".ppm");
    const auto notes = LuaRect(lua, "App.notesWindow");
    UIRect overlap;
    overlap.x = std::max(canvas.x, notes.x) + 2;
    overlap.y = std::max(canvas.y, notes.y) + 2;
    overlap.width = std::min(canvas.x + canvas.width, notes.x + notes.width) - overlap.x - 2;
    overlap.height = std::min(canvas.y + canvas.height, notes.y + notes.height) - overlap.y - 2;
    Require(overlap.width > 10 && overlap.height > 10, "Window smoke needs overlapping canvas and scratchpad");
    Require(differences(previewOnly, notesFront, overlap) > 100, "Scratchpad did not occlude canvas");
    auto window = LuaRect(lua, "App.canvasWindow");
    PointerClick(manager, window.x + 20, window.y + 16);
    Render(manager, renderer, true);
    Require(differences(previewOnly, renderer.GetCapture(), overlap) == 0, "Click-to-front did not raise the canvas above scratchpad");
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-canvas-front-" + size + ".ppm");
    if (notes.x + notes.width > window.x + window.width + 2)
        PointerClick(manager, notes.x + notes.width - 3, notes.y + 40);
    else
        PointerClick(manager, notes.x + 20, notes.y + notes.height - 3);
    Render(manager, renderer, true);
    Require(differences(notesFront, renderer.GetCapture(), overlap) == 0, "Click-to-front did not restore scratchpad above canvas");
    PointerClick(manager, window.x + 20, window.y + 16);
    manager.InjectMouseMove(window.x + 20, window.y + 16);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(window.x + 33, window.y + 39);
    manager.InjectMouseButton(MouseButton::Left, false); manager.Update(0);
    auto moved = LuaRect(lua, "App.canvasWindow");
    Require(std::abs(moved.x - window.x - 13) < 0.1f && std::abs(moved.y - window.y - 23) < 0.1f, "Window title drag failed");
    manager.InjectMouseMove(moved.x + moved.width - 2, moved.y + moved.height - 2);
    Require(manager.GetPointerShape() == PointerShape::ResizeNWSE, "Window corner cursor mismatch");
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(moved.x + moved.width - 22, moved.y + moved.height - 18);
    Require(manager.GetPointerShape() == PointerShape::ResizeNWSE, "Captured resize cursor changed direction");
    manager.InjectMouseButton(MouseButton::Left, false); manager.Update(0);
    window = LuaRect(lua, "App.canvasWindow");
    Require(std::abs(window.width - moved.width + 20) < 0.1f && std::abs(window.height - moved.height + 16) < 0.1f, "Window corner resize failed");
    Render(manager, renderer, true);
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-canvas-resized-" + size + ".ppm");
    PointerClick(manager, window.x + window.width - 77, window.y + 16);
    Execute(lua, "assert(App.canvasWindow:GetWindowState() == 'Minimized')");
    Render(manager, renderer, true);
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-canvas-minimized-" + size + ".ppm");
    PointerClick(manager, window.x + window.width - 77, window.y + 16);
    Execute(lua, "assert(App.canvasWindow:GetWindowState() == 'Normal')");
    PointerClick(manager, window.x + window.width - 49, window.y + 16);
    Execute(lua, "assert(App.canvasWindow:GetWindowState() == 'Maximized')");
    manager.Update(0); Render(manager, renderer, true);
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-canvas-maximized-" + size + ".ppm");
    window = LuaRect(lua, "App.canvasWindow");
    PointerClick(manager, window.x + window.width - 21, window.y + 16);
    Execute(lua, "assert(not App.canvasWindow:IsVisible())");
    Click(lua, manager, "showCanvas");
    Execute(lua, "assert(App.canvasWindow:IsVisible() and App.canvasWindow:GetWindowState() == 'Normal')");
    Click(lua, manager, "pauseField");
    Execute(lua, "assert(not App.fieldPaused)");
    Click(lua, manager, "arrangeWindows");
    Execute(lua, "assert(App.canvasWindow:IsVisible() and App.notesWindow:IsVisible())");
    Click(lua, manager, "closeNotes");
    Execute(lua, "assert(not App.notesWindow:IsVisible()); App.canvasWindow:Close()");
    manager.Update(0);
}

void Smoke(lua_State* lua, UIManager& manager, VulkanExampleRenderer& renderer, const std::string& prefix, const std::string& size) {
    LAMBUI_LOGI(TAG, "Checking Lua application at {}", size);
    Click(lua, manager, "route2");
    Execute(lua, "assert(App.selected == 2)");
    Click(lua, manager, "launch");
    Execute(lua, "assert(not App.running and App.status:GetText() == 'Complete the readiness check first.')");
    Click(lua, manager, "ready");
    Execute(lua, "assert(App.ready:IsChecked())");
    Click(lua, manager, "notes");
    manager.InjectKeyEvent(ScanCode::LeftControl, true);
    manager.InjectKeyEvent(ScanCode::A, true); manager.InjectKeyEvent(ScanCode::A, false);
    manager.InjectKeyEvent(ScanCode::LeftControl, false);
    for (const char* text = "Meet at the north gate."; *text; ++text) manager.InjectCharacter(*text);
    Click(lua, manager, "save");
    Execute(lua, "assert(App.routes[2].notes == 'Meet at the north gate.')");
    Click(lua, manager, "launch");
    Call(lua, "Update", {3.0}); manager.Update(0);
    Execute(lua, "assert(App.running and App.progress:GetValue() > 0)");
    Click(lua, manager, "launch");
    Execute(lua, "assert(not App.running and App.progress:GetValue() > 0)");
    Render(manager, renderer, true);
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-active-" + size + ".ppm");
    Click(lua, manager, "launch");
    Call(lua, "Update", {120.0}); manager.Update(0);
    Execute(lua, "assert(not App.running and App.progress:GetValue() == 100)");
    Click(lua, manager, "reset");
    Execute(lua, "assert(App.progress:GetValue() == 0 and not App.running)");
    Click(lua, manager, "route1");
    Execute(lua, "App.viewport:SetScrollOffset(0,0)"); manager.Update(0);
    Render(manager, renderer, true);
    const auto first = renderer.GetCapture();
    Require(std::count_if(first.begin(), first.end(), [](uint8_t value) { return value > 80 && value < 240; }) > first.size()/40,
        "Application frame is missing colored UI content");
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-" + size + ".ppm");
    manager.InjectMouseMove(150, 300);
    manager.InjectMouseWheel(0, -400); manager.Update(0);
    Render(manager, renderer, true);
    if (size != "desktop") Require(first != renderer.GetCapture(), "Compact workspace did not scroll");
    if (!prefix.empty()) renderer.SaveCapture(prefix + "-scrolled-" + size + ".ppm");
    Execute(lua, "App.viewport:SetScrollOffset(0,0)"); manager.Update(0);
}

int Run(GLFWwindow* window, const std::string& fontPath, const std::string& headingPath,
        const std::string& script, const std::string& screenshot, bool smoke, bool validation) {
    LAMBUI_LOGT(TAG, "Start Lua application");
    FontAtlas font, heading;
    Require(font.LoadFromFile(FindFont(fontPath, false), 20), "Cannot load body font");
    Require(heading.LoadFromFile(FindFont(headingPath, true), 30), "Cannot load heading font");
    auto measurer = std::make_shared<FontAtlasTextMeasurer>(font);
    void* headingHandle = reinterpret_cast<void*>(uintptr_t{1});
    measurer->RegisterFont(headingHandle, heading);
    auto renderer = std::make_shared<VulkanExampleRenderer>(window, validation);
    renderer->LoadFont(font); renderer->LoadFont(heading, headingHandle);
    const auto map = renderer->UploadTexture(768, 360, Terrain(768, 360));
    UIManager manager(renderer, measurer);
    LambUIExamples::GlfwPointer pointer(window);
    if (smoke) Require(pointer.SmokeTest(), "Native cursor smoke failed");
    std::unique_ptr<lua_State, decltype(&lua_close)> lua(luaL_newstate(), lua_close);
    Require(lua != nullptr, "Cannot create Lua state");
    luaL_openlibs(lua.get());
    LambUILua::LuaUIBindings bindings(lua.get(), manager);
    lua_newtable(lua.get());
    lua_pushinteger(lua.get(), static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(map)));
    lua_setfield(lua.get(), -2, "map");
    lua_pushinteger(lua.get(), 1); lua_setfield(lua.get(), -2, "headingFont");
    lua_pushlightuserdata(lua.get(), renderer.get());
    lua_pushcclosure(lua.get(), DrawField, 1); lua_setfield(lua.get(), -2, "DrawField");
    lua_setglobal(lua.get(), "Host");
    const int load = luaL_dofile(lua.get(), script.c_str());
    Require(load == LUA_OK, load == LUA_OK ? "" : (lua_tostring(lua.get(), -1) ? lua_tostring(lua.get(), -1) : "Lua script failed"));
    ConnectInput(window, manager);
    int previousWidth = 0, previousHeight = 0;
    auto resize = [&]() {
        int width = 0, height = 0;
        glfwGetWindowSize(window, &width, &height);
        if (width != previousWidth || height != previousHeight) {
            LAMBUI_LOGT(TAG, "Resize Lua layout {}x{}", width, height);
            manager.SetDisplaySize(float(width), float(height));
            Call(lua.get(), "Resize", {double(width), double(height)});
            previousWidth = width; previousHeight = height;
        }
        manager.Update(0);
    };
    resize();
    if (smoke) {
        for (const auto& dimensions : std::vector<std::pair<int,int>>{{1100,780}, {420,780}, {360,480}, {1100,780}}) {
            glfwSetWindowSize(window, dimensions.first, dimensions.second);
            glfwPollEvents(); resize();
            CheckRenderer(*renderer, previousWidth, previousHeight);
            Smoke(lua.get(), manager, *renderer, screenshot,
                dimensions.first == 360 ? "minimum" : (dimensions.first < 600 ? "compact" : "desktop"));
            SmokeWindows(lua.get(), manager, *renderer, screenshot,
                dimensions.first == 360 ? "minimum" : (dimensions.first < 600 ? "compact" : "desktop"));
        }
        Require(renderer->GetValidationErrors() == 0, "Vulkan validation reported errors");
        glfwSetWindowUserPointer(window, nullptr);
        LAMBUI_LOGI(TAG, "Vulkan/Lua smoke checks passed");
        return 0;
    }
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); resize();
        const double now = glfwGetTime();
        const double delta = std::min(0.1, now - previous); previous = now;
        Call(lua.get(), "Update", {delta});
        manager.Update(float(delta));
        Require(pointer.Apply(manager.GetPointerShape()), "Cannot apply native pointer shape");
        if (renderer->BeginFrame()) {
            manager.Render(); renderer->EndFrame(!screenshot.empty());
            if (!screenshot.empty()) { renderer->SaveCapture(screenshot + ".ppm"); break; }
        } else glfwWaitEvents();
    }
    glfwSetWindowUserPointer(window, nullptr);
    return 0;
}
}

int main(int argc, char** argv) {
    Log::SetMinLevel(LogLevel::Info);
    unsigned errors = 0;
    Log::SetCallback([&errors](LogLevel level, const char* tag, const std::string& message) {
        if (level == LogLevel::Error || message.find("Validation Error") != std::string::npos) ++errors;
        std::cerr << '[' << ToString(level) << "] " << tag << ": " << message << '\n';
    });
    GLFWwindow* window = nullptr;
    bool initialized = false;
    int result = 1;
    try {
        LAMBUI_LOGT(TAG, "Construct application");
        std::string font, heading, screenshot, script = std::string(LAMBUI_VULKAN_ASSET_DIR) + "/planner.lua";
        bool smoke = false, validation = false;
        int width = 1100, height = 780;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--smoke-test") smoke = true;
            else if (argument == "--validation") validation = true;
            else if (argument == "--font" || argument == "--heading-font" || argument == "--script" ||
                     argument == "--screenshot" || argument == "--width" || argument == "--height") {
                Require(index + 1 < argc, "Missing value for " + argument);
                const std::string value = argv[++index];
                if (argument == "--font") font = value;
                else if (argument == "--heading-font") heading = value;
                else if (argument == "--script") script = value;
                else if (argument == "--screenshot") screenshot = value;
                else if (argument == "--width") width = std::stoi(value);
                else height = std::stoi(value);
            } else throw std::runtime_error("Unknown argument: " + argument);
        }
        Require(width >= 360 && height >= 480, "Minimum window size is 360x480");
        Require(glfwInit() != 0, "Cannot initialize GLFW"); initialized = true;
        Require(glfwVulkanSupported() != 0, "Vulkan loader/driver unavailable");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Fieldwork | LambUI Vulkan + Lua", nullptr, nullptr);
        Require(window != nullptr, "Cannot create GLFW window");
        glfwSetWindowSizeLimits(window, 360, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
        result = Run(window, font, heading, script, screenshot, smoke, validation);
        if (errors) result = 1;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan example failed: " << error.what() << '\n';
    }
    if (window) glfwDestroyWindow(window);
    if (initialized) glfwTerminate();
    LAMBUI_LOGT(TAG, "Destroy application");
    Log::SetCallback({});
    return result;
}