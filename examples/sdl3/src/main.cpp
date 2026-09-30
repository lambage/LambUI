#include "SDLExampleRenderer.h"
#include "LambUI/LambUI.h"

#include <SDL3/SDL.h>
#include <iostream>
#include <memory>

using namespace LambUI;

int main() {
    LambUI::Log::UseDefaultConsoleSink();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("LambUI - SDL3 Example", 1280, 720, 0, &window, &sdlRenderer)) {
        std::cerr << "Failed to create window/renderer: " << SDL_GetError() << "\n";
        SDL_Quit();
        return -1;
    }

    auto renderer = std::make_shared<SDLExampleRenderer>(sdlRenderer);

    // Demo-only: loads a local system font. Real consumers should ship/point
    // at their own TTF asset; FontAtlas::LoadFromFile takes any TTF/OTF path.
    auto fontAtlas = std::make_shared<FontAtlas>();
    std::shared_ptr<FontAtlasTextMeasurer> textMeasurer;
    if (fontAtlas->LoadFromFile("C:/Windows/Fonts/segoeui.ttf")) {
        renderer->LoadFont(*fontAtlas);
        textMeasurer = std::make_shared<FontAtlasTextMeasurer>(*fontAtlas);
    }

    UIManager uiManager(renderer, textMeasurer);
    uiManager.SetDisplaySize(1280.0f, 720.0f);

    SDL_StartTextInput(window);

    // --- Build the same small demo UI tree as the other backends ---
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

    UITextWidget* label = panel->CreateChild<UITextWidget>("DemoLabel");
    label->SetTextMeasurer(uiManager.GetTextMeasurer());
    label->SetText("Hello, LambUI!");
    label->SetPoint(AnchorPoint::Top, slider, AnchorPoint::Bottom, 0.0f, 16.0f);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    uiManager.InjectMouseMove(event.motion.x, event.motion.y);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    MouseButton mapped = MouseButton::Left;
                    if (event.button.button == SDL_BUTTON_RIGHT) mapped = MouseButton::Right;
                    else if (event.button.button == SDL_BUTTON_MIDDLE) mapped = MouseButton::Middle;
                    uiManager.InjectMouseButton(mapped, event.button.down);
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    // SDL's keycodes for control characters (backspace/enter/escape)
                    // are numerically equal to their ASCII values, matching LambUI::ScanCode.
                    uiManager.InjectKeyEvent(static_cast<uint32_t>(event.key.key), event.key.down);
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    // Simplified: only forwards ASCII bytes for this demo.
                    for (const char* c = event.text.text; *c != '\0'; ++c) {
                        if (static_cast<unsigned char>(*c) < 0x80) {
                            uiManager.InjectCharacter(static_cast<char32_t>(*c));
                        }
                    }
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    uiManager.SetDisplaySize(static_cast<float>(event.window.data1),
                                              static_cast<float>(event.window.data2));
                    break;
                default:
                    break;
            }
        }

        uiManager.Update(0.0f);

        SDL_SetRenderDrawColor(sdlRenderer, 25, 25, 30, 255);
        SDL_RenderClear(sdlRenderer);

        uiManager.Render();

        SDL_RenderPresent(sdlRenderer);
    }

    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
