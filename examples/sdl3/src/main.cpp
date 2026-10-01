#include "SDLExampleRenderer.h"
#include "../../common/WidgetShowcase.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <string>

using namespace LambUI;

namespace {
constexpr const char* TAG = "SDLExample";

bool CheckPixels(SDL_Renderer* renderer, const std::string& screenshotPath, Uint64& digest) {
    LAMBUI_LOGT(TAG, "CheckPixels('{}')", screenshotPath);
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface) {
        LAMBUI_LOGE(TAG, "ReadPixels: {}", SDL_GetError());
        return false;
    }
    bool passed = true;
    digest = 14695981039346656037ull;
    if (!screenshotPath.empty()) passed = SDL_SaveBMP(surface, screenshotPath.c_str());
    size_t distinct = 0;
    Uint8 baseRed = 0, baseGreen = 0, baseBlue = 0, baseAlpha = 0;
    passed = SDL_ReadSurfacePixel(surface, 0, 0, &baseRed, &baseGreen, &baseBlue, &baseAlpha) && passed;
    for (int row = 0; row < surface->h; row += 8) {
        for (int column = 0; column < surface->w; column += 8) {
            Uint8 red = 0, green = 0, blue = 0, alpha = 0;
            if (!SDL_ReadSurfacePixel(surface, column, row, &red, &green, &blue, &alpha)) passed = false;
            digest = (digest ^ red) * 1099511628211ull;
            digest = (digest ^ green) * 1099511628211ull;
            digest = (digest ^ blue) * 1099511628211ull;
            if (red != baseRed || green != baseGreen || blue != baseBlue) ++distinct;
        }
    }
    SDL_DestroySurface(surface);
    passed = distinct > 100 && passed;
    LAMBUI_LOGI(TAG, "Pixel smoke: {} ({} non-background samples)", passed ? "PASS" : "FAIL", distinct);
    return passed;
}

bool CheckClipping(SDL_Renderer* renderer, SDLExampleRenderer& adapter) {
    LAMBUI_LOGT(TAG, "CheckClipping");
    bool passed = true;
    for (const UIRect hidden : {UIRect{80, 16, 16, 16}, UIRect{16, 80, 16, 16},
                               UIRect{-40, 16, 16, 16}, UIRect{16, -40, 16, 16},
                               UIRect{24, 24, 0, 16}}) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        UIRenderCommand outer, inner, fill, text, callback, pop, marker;
        outer.type = inner.type = RenderCommandType::PushScissor;
        outer.x = outer.y = 16;
        outer.width = outer.height = 32;
        inner.x = hidden.x;
        inner.y = hidden.y;
        inner.width = hidden.width;
        inner.height = hidden.height;
        fill.width = fill.height = 64;
        fill.color = 0xFF0000FFu;
        text.type = RenderCommandType::DrawString;
        text.text = "Clipped";
        callback.type = RenderCommandType::CustomCallback;
        callback.customRenderFunc = [renderer](const UICustomRenderArgs&) {
            LAMBUI_LOGT(TAG, "Clipping probe canvas");
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            const SDL_FRect bounds{0, 0, 64, 64};
            SDL_RenderFillRect(renderer, &bounds);
        };
        pop.type = RenderCommandType::PopScissor;
        marker.x = marker.y = 20;
        marker.width = marker.height = 4;
        marker.color = 0x0000FFFFu;
        adapter.SubmitRenderCommands({outer, fill, inner, fill, text, callback, outer,
                                       fill, pop, pop, marker, pop});
        marker.x = marker.y = 4;
        marker.color = 0xFFFFFFFFu;
        adapter.SubmitRenderCommands({marker});
        const SDL_Rect region{0, 0, 64, 64};
        SDL_Surface* surface = SDL_RenderReadPixels(renderer, &region);
        if (!surface) return false;
        for (int row = 0; row < 64; ++row) {
            for (int column = 0; column < 64; ++column) {
                Uint8 red = 0, green = 0, blue = 0, alpha = 0;
                const bool parent = column >= 16 && column < 48 && row >= 16 && row < 48;
                const bool restored = column >= 20 && column < 24 && row >= 20 && row < 24;
                const bool unclipped = column >= 4 && column < 8 && row >= 4 && row < 8;
                passed = SDL_ReadSurfacePixel(surface, column, row, &red, &green, &blue, &alpha) && passed;
                passed = red == ((parent && !restored) || unclipped ? 255 : 0) &&
                         green == (unclipped ? 255 : 0) &&
                         blue == (restored || unclipped ? 255 : 0) && passed;
            }
        }
        SDL_DestroySurface(surface);
    }
    LAMBUI_LOGI(TAG, "Nested empty-clip pixels and restoration: {}", passed ? "PASS" : "FAIL");
    return passed;
}

bool CheckFonts(SDL_Renderer* renderer, SDLExampleRenderer& adapter, void* headingFont) {
    LAMBUI_LOGT(TAG, "CheckFonts");
    UIRenderCommand text;
    text.type = RenderCommandType::DrawString;
    text.text = "LambUI 0123";
    text.x = text.y = 8.0f;
    const auto capture = [&](void* fontHandle, Uint64& digest) {
        text.fontHandle = fontHandle;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        adapter.SubmitRenderCommands({text});
        const SDL_Rect region{0, 0, 320, 64};
        SDL_Surface* surface = SDL_RenderReadPixels(renderer, &region);
        if (!surface) return false;
        digest = 14695981039346656037ull;
        size_t ink = 0;
        bool valid = true;
        for (int row = 0; row < surface->h; ++row) {
            for (int column = 0; column < surface->w; ++column) {
                Uint8 red = 0, green = 0, blue = 0, alpha = 0;
                valid = SDL_ReadSurfacePixel(surface, column, row, &red, &green, &blue, &alpha) && valid;
                digest = (digest ^ red) * 1099511628211ull;
                if (red > 0) ++ink;
            }
        }
        SDL_DestroySurface(surface);
        return valid && ink > 20;
    };
    Uint64 bodyPixels = 0, headingPixels = 0, fallbackPixels = 0;
    const bool passed = capture(nullptr, bodyPixels) && capture(headingFont, headingPixels) &&
        capture(&text, fallbackPixels) && bodyPixels != headingPixels && bodyPixels == fallbackPixels;
    LAMBUI_LOGI(TAG, "Multiple-font pixels and fallback: {}", passed ? "PASS" : "FAIL");
    return passed;
}

int RunExample(SDL_Window* window, SDL_Renderer* sdlRenderer, const std::string& fontPath,
               const std::string& headingFontPath,
               bool smoke, const std::string& screenshotPrefix) {
    LAMBUI_LOGT(TAG, "RunExample(smoke={})", smoke);
    FontAtlas fontAtlas;
    auto renderer = std::make_shared<SDLExampleRenderer>(sdlRenderer);
    if (!fontAtlas.LoadFromFile(fontPath, 20) || !renderer->LoadFont(fontAtlas)) {
        LAMBUI_LOGE(TAG, "Cannot load font '{}'; pass --font <path-to-ttf>", fontPath);
        return 1;
    }
    auto textMeasurer = std::make_shared<FontAtlasTextMeasurer>(fontAtlas);
    FontAtlas headingAtlas;
    if (!headingAtlas.LoadFromFile(headingFontPath, 26) || !renderer->LoadFont(headingAtlas, &headingAtlas) ||
        !textMeasurer->RegisterFont(&headingAtlas, headingAtlas)) {
        LAMBUI_LOGE(TAG, "Cannot register heading font '{}'", headingFontPath);
        return 1;
    }
    UIManager uiManager(renderer, textMeasurer);
    uiManager.SetClipboardCallbacks([](std::string& text) {
        LAMBUI_LOGT(TAG, "ReadClipboard");
        if (!SDL_HasClipboardText()) return false;
        char* value = SDL_GetClipboardText();
        if (!value) return false;
        text = value;
        SDL_free(value);
        return true;
    }, [](const std::string& text) {
        LAMBUI_LOGT(TAG, "WriteClipboard(bytes={})", text.size());
        return SDL_SetClipboardText(text.c_str());
    });
    SDL_StartTextInput(window);
    auto* title = uiManager.GetRoot().CreateChild<UITextWidget>("Title");
    title->SetMouseEnabled(false);
    title->SetTextMeasurer(uiManager.GetTextMeasurer());
    title->SetFont(&headingAtlas);
    title->SetText("LambUI / Asset Browser");
    title->SetPoint(AnchorPoint::TopLeft, &uiManager.GetRoot(), AnchorPoint::TopLeft, 24.0f, 20.0f);
    title->SetColor(0xB9E5D8FFu);
    LambUIExamples::WidgetShowcase widgets(uiManager, [sdlRenderer](const UICustomRenderArgs& args) {
        const UIRect rect{args.viewportX, args.viewportY, args.viewportWidth, args.viewportHeight};
        SDL_SetRenderDrawColor(sdlRenderer, 18, 44, 40, 255);
        SDL_FRect background{rect.x, rect.y, rect.width, rect.height};
        SDL_RenderFillRect(sdlRenderer, &background);
        SDL_SetRenderDrawColor(sdlRenderer, 103, 219, 179, 255);
        for (int band = 0; band < 14; ++band) {
            SDL_FPoint points[65];
            for (int sample = 0; sample <= 64; ++sample) {
                const float phase = static_cast<float>(sample) / 64.0f;
                points[sample] = {rect.x + phase * rect.width,
                    rect.y + rect.height * (0.08f + band * 0.06f + 0.04f * std::sin(phase * 12.0f + band * 0.6f))};
            }
            SDL_RenderLines(sdlRenderer, points, 65);
        }
    });
    bool running = true;
    bool passed = true;
    bool screenshotSaved = false;
    int smokeFrame = 0;
    int previousWidth = -1, previousHeight = -1;
    Uint64 previousTime = SDL_GetTicksNS();
    while (running) {
        if (smoke) {
            if (!SDL_SetWindowSize(window, smokeFrame == 0 ? 920 : 420, 680) || !SDL_SyncWindow(window)) return 1;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    uiManager.InjectMouseMove(event.motion.x, event.motion.y);
                    break;
                case SDL_EVENT_MOUSE_WHEEL: {
                    const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                    uiManager.InjectMouseMove(event.wheel.mouse_x, event.wheel.mouse_y);
                    uiManager.InjectMouseWheel(event.wheel.x * direction * 20.0f,
                                              event.wheel.y * direction * 20.0f);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    MouseButton mapped = MouseButton::Left;
                    if (event.button.button == SDL_BUTTON_RIGHT) mapped = MouseButton::Right;
                    else if (event.button.button == SDL_BUTTON_MIDDLE) mapped = MouseButton::Middle;
                    else if (event.button.button != SDL_BUTTON_LEFT) break;
                    uiManager.InjectMouseMove(event.button.x, event.button.y);
                    uiManager.InjectMouseButton(mapped, event.button.down);
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP: {
                    LAMBUI_LOGT(TAG, "Key({}, {})", event.key.key, event.key.down);
                    uint32_t scanCode = 0;
                    switch (event.key.key) {
                        case SDLK_BACKSPACE: scanCode = ScanCode::Backspace; break;
                        case SDLK_RETURN: case SDLK_KP_ENTER: scanCode = ScanCode::Enter; break;
                        case SDLK_ESCAPE: scanCode = ScanCode::Escape; break;
                        case SDLK_SPACE: scanCode = ScanCode::Space; break;
                        case SDLK_TAB: scanCode = ScanCode::Tab; break;
                        case SDLK_LSHIFT: scanCode = ScanCode::LeftShift; break;
                        case SDLK_RSHIFT: scanCode = ScanCode::RightShift; break;
                        case SDLK_LCTRL: scanCode = ScanCode::LeftControl; break;
                        case SDLK_RCTRL: scanCode = ScanCode::RightControl; break;
                        case SDLK_A: scanCode = ScanCode::A; break;
                        case SDLK_C: scanCode = ScanCode::C; break;
                        case SDLK_X: scanCode = ScanCode::X; break;
                        case SDLK_V: scanCode = ScanCode::V; break;
                        case SDLK_LEFT: scanCode = ScanCode::Left; break;
                        case SDLK_RIGHT: scanCode = ScanCode::Right; break;
                        case SDLK_UP: scanCode = ScanCode::Up; break;
                        case SDLK_DOWN: scanCode = ScanCode::Down; break;
                        case SDLK_HOME: scanCode = ScanCode::Home; break;
                        case SDLK_END: scanCode = ScanCode::End; break;
                        case SDLK_DELETE: scanCode = ScanCode::Delete; break;
                        default: break;
                    }
                    if (scanCode) uiManager.InjectKeyEvent(scanCode, event.key.down);
                    break;
                }
                case SDL_EVENT_TEXT_INPUT:
                    for (const char* character = event.text.text; *character != '\0'; ++character) {
                        if (static_cast<unsigned char>(*character) < 0x80) {
                            uiManager.InjectCharacter(static_cast<char32_t>(*character));
                        }
                    }
                    break;
                default:
                    break;
            }
        }

        if (!running) break;
        int width = 0, height = 0;
        SDL_GetWindowSize(window, &width, &height);
        if (width != previousWidth || height != previousHeight) {
            LAMBUI_LOGT(TAG, "Resize({}, {})", width, height);
            uiManager.SetDisplaySize(static_cast<float>(width), static_cast<float>(height));
            widgets.Layout(24.0f, 64.0f, static_cast<float>(width) - 48.0f, static_cast<float>(height) - 88.0f);
            previousWidth = width;
            previousHeight = height;
        }
        const Uint64 now = SDL_GetTicksNS();
        const float deltaTime = std::min(static_cast<float>(now - previousTime) / 1.0e9f, 0.1f);
        previousTime = now;
        widgets.Update(deltaTime);
        uiManager.Update(deltaTime);
        if (smoke) {
            passed = widgets.SmokeTest() && passed;
            passed = CheckClipping(sdlRenderer, *renderer) && passed;
            passed = CheckFonts(sdlRenderer, *renderer, &headingAtlas) && passed;
        }
        SDL_SetRenderDrawColor(sdlRenderer, 20, 27, 31, 255);
        SDL_RenderClear(sdlRenderer);
        uiManager.Render();
        const std::string suffix = width < 600 ? "-compact.bmp" : "-desktop.bmp";
        Uint64 basePixels = 0;
        if (smoke || (!screenshotPrefix.empty() && !screenshotSaved)) {
            passed = CheckPixels(sdlRenderer, screenshotPrefix.empty() ? "" : screenshotPrefix + suffix, basePixels) && passed;
            screenshotSaved = true;
        }
        if (smoke) {
            passed = widgets.CaptureViews([&](const char* state) {
                SDL_SetRenderDrawColor(sdlRenderer, 20, 27, 31, 255);
                SDL_RenderClear(sdlRenderer);
                uiManager.Render();
                Uint64 statePixels = 0;
                const bool valid = CheckPixels(sdlRenderer, screenshotPrefix.empty() ? "" : screenshotPrefix + "-" + state + suffix, statePixels);
                return valid && statePixels != basePixels;
            }) && passed;
            LAMBUI_LOGI(TAG, "Showcase smoke {}x{}: {}", width, height, passed ? "PASS" : "FAIL");
        }
        SDL_RenderPresent(sdlRenderer);
        if (smoke && ++smokeFrame == 2) break;
    }
    SDL_StopTextInput(window);
    return passed ? 0 : 1;
}
}

int main(int argc, char** argv) {
    Log::UseDefaultConsoleSink();
    Log::SetMinLevel(LogLevel::Info);
    LAMBUI_LOGI(TAG, "Starting SDL3 widget showcase");
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
            LAMBUI_LOGE(TAG, "Usage: lambui_example_sdl3 [--font path] [--heading-font path] [--smoke-test] [--screenshot prefix]");
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
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LAMBUI_LOGE(TAG, "SDL_Init: {}", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (smoke ? SDL_WINDOW_HIDDEN : 0);
    if (!SDL_CreateWindowAndRenderer("LambUI - SDL3 Asset Browser", 920, 680, flags, &window, &sdlRenderer)) {
        LAMBUI_LOGE(TAG, "CreateWindowAndRenderer: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 360, 480);
    SDL_SetRenderVSync(sdlRenderer, smoke ? 0 : 1);
    const int result = RunExample(window, sdlRenderer, fontPath, headingFontPath, smoke, screenshotPrefix);

    LAMBUI_LOGT(TAG, "Destroy renderer/window and quit SDL");
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
