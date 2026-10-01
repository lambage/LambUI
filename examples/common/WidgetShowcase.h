#pragma once

#include "LambUI/LambUI.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace LambUIExamples {

class WidgetShowcase {
public:
    explicit WidgetShowcase(LambUI::UIManager& manager, LambUI::UICustomRenderCallback preview) : m_manager(manager) {
        using namespace LambUI;
        LAMBUI_LOGT(TAG, "constructed");
        m_panel = manager.GetRoot().CreateChild<UIWidget>("WidgetShowcase");
        auto* background = m_panel->CreateChild<UITextureWidget>("ShowcaseBackground");
        background->SetAllPoints(m_panel);
        background->SetMouseEnabled(false);
        background->SetTint(0x242A2DFFu);
        m_menuBar = m_panel->CreateChild<UIMenuBar>(manager, "ShowcaseMenu");
        m_menuBar->SetPoint(AnchorPoint::TopLeft, m_panel, AnchorPoint::TopLeft);
        m_menuBar->SetPoint(AnchorPoint::TopRight, m_panel, AnchorPoint::TopRight);
        m_menuBar->SetSize(0.0f, 28.0f);
        m_tabs = m_panel->CreateChild<UITabControl>("ShowcaseTabs");
        m_tabs->SetPoint(AnchorPoint::TopLeft, m_panel, AnchorPoint::TopLeft, 8.0f, 36.0f);
        m_tabs->SetPoint(AnchorPoint::BottomRight, m_panel, AnchorPoint::BottomRight, -8.0f, -64.0f);
        auto* assets = m_tabs->AddTab("Assets");
        auto* job = m_tabs->AddTab("Build");
        m_tree = assets->CreateChild<UITreeView>("AssetTree");
        m_tree->SetAllPoints(assets);
        m_tree->SetTooltip("Project assets");
        for (const auto& group : std::vector<std::vector<std::string>>{
                 {"Materials", "Contour", "Brushed metal", "Glass", "Stone"},
                 {"Textures", "Albedo", "Normal", "Roughness", "Emissive"},
                 {"Scenes", "Atrium", "Courtyard", "Workshop", "Gallery"},
                 {"Animations", "Idle", "Walk", "Run", "Jump", "Land"}}) {
            const auto folder = m_tree->AddNode(UITreeView::RootNode, group.front());
            m_folders.push_back(folder);
            m_names.push_back(group.front());
            for (size_t index = 1; index < group.size(); ++index) {
                m_tree->AddNode(folder, group[index]);
                m_names.push_back(group[index]);
            }
        }
        m_selection = AddLabel(*job, "Selection", "Contour", 12.0f, 12.0f);
        AddLabel(*job, "JobType", "Asset preview build", 12.0f, 40.0f);
        m_toggle = job->CreateChild<UIButton>("ToggleBuild");
        m_toggle->SetPoint(AnchorPoint::TopLeft, job, AnchorPoint::TopLeft, 12.0f, 78.0f);
        m_toggle->SetSize(112.0f, 32.0f);
        m_toggle->SetNormalColor(0x286550FFu);
        m_toggle->SetHoverColor(0x347F66FFu);
        m_toggle->SetPressedColor(0x1B493BFFu);
        m_toggle->SetTooltip("Pause or resume the preview build");
        m_toggleText = AddLabel(*m_toggle, "ToggleLabel", "Pause", 10.0f, 4.0f);
        m_toggle->RegisterCallback(UIEventType::OnClick, [this](const UIEventData& event) {
            LAMBUI_LOGT(TAG, "ToggleBuild click");
            event.handled = true;
            if (event.button == MouseButton::Left) SetRunning(!m_running);
        });
        m_reset = job->CreateChild<UIButton>("ResetBuild");
        m_reset->SetPoint(AnchorPoint::TopLeft, job, AnchorPoint::TopLeft, 136.0f, 78.0f);
        m_reset->SetSize(100.0f, 32.0f);
        m_reset->SetNormalColor(0x465159FFu);
        m_reset->SetHoverColor(0x596872FFu);
        m_reset->SetPressedColor(0x303A40FFu);
        AddLabel(*m_reset, "ResetLabel", "Restart", 10.0f, 4.0f);
        m_reset->RegisterCallback(UIEventType::OnClick, [this](const UIEventData& event) {
            LAMBUI_LOGT(TAG, "Restart click");
            event.handled = true;
            if (event.button == MouseButton::Left) Restart();
        });
        m_status = AddLabel(*m_panel, "BuildStatus", "", 12.0f, 0.0f);
        m_status->ClearPoints();
        m_status->SetPoint(AnchorPoint::TopLeft, m_panel, AnchorPoint::BottomLeft, 12.0f, -54.0f);
        m_progress = m_panel->CreateChild<UIProgressBar>("BuildProgress");
        m_progress->SetPoint(AnchorPoint::TopLeft, m_panel, AnchorPoint::BottomLeft, 12.0f, -24.0f);
        m_progress->SetPoint(AnchorPoint::BottomRight, m_panel, AnchorPoint::BottomRight, -12.0f, -12.0f);
        m_progress->SetMinMaxValues(0.0f, 100.0f);
        m_progress->SetColors(0x101619FFu, 0x67DBB3FFu);
        m_progress->RegisterCallback(UIEventType::OnValueChanged, [this](const UIEventData&) { RefreshStatus(); });
        m_tree->RegisterCallback(UIEventType::OnValueChanged, [this](const UIEventData&) {
            LAMBUI_LOGT(TAG, "Selection({})", m_tree->GetSelectedNode());
            const auto selected = m_tree->GetSelectedNode();
            if (selected > 0 && selected <= m_names.size()) m_selection->SetText(m_names[selected - 1]);
        });
        m_context = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "AssetActions");
        m_context->SetItems({
            {"Build selected", [this] { Restart(); m_tabs->SetSelectedIndex(1); }},
            {"", {}, true, true},
            {"Collapse folders", [this] { SetExpanded(false); }},
            {"Expand folders", [this] { SetExpanded(true); }},
            {"Delete (read-only)", {}, false}});
        m_tree->RegisterCallback(UIEventType::OnClick, [this](const UIEventData& event) {
            LAMBUI_LOGT(TAG, "Asset click({})", ToString(event.button));
            if (event.button == MouseButton::Right) {
                event.handled = true;
                m_context->Open(event.mouseX, event.mouseY, m_tree);
            }
        });
        m_menuBar->AddMenu("View", {
            {"Assets", [this] { m_tabs->SetSelectedIndex(0); }},
            {"Build job", [this] { m_tabs->SetSelectedIndex(1); }},
            {"Controls", [this] { m_tabs->SetSelectedIndex(2); }},
            {"Window settings", [this] { m_tabs->SetSelectedIndex(3); }},
            {"Open preview", [this] { OpenWindow(); }}});
        m_menuBar->AddMenu("Build", {
            {"Restart", [this] { Restart(); }},
            {"Pause", [this] { SetRunning(false); }},
            {"Resume", [this] { SetRunning(true); }}});
        m_tree->SetSelectedNode(2);
        m_progress->SetValue(35.0f);
        BuildGallery(std::move(preview));
    }

    ~WidgetShowcase() { LAMBUI_LOGT(TAG, "destroyed"); }
    WidgetShowcase(const WidgetShowcase&) = delete;
    WidgetShowcase& operator=(const WidgetShowcase&) = delete;

    void Layout(float x, float y, float width, float height) {
        LAMBUI_LOGT(TAG, "Layout({}, {}, {}, {})", x, y, width, height);
        m_panel->ClearPoints();
        m_panel->SetPoint(LambUI::AnchorPoint::TopLeft, &m_manager.GetRoot(), LambUI::AnchorPoint::TopLeft, x, y);
        m_panel->SetSize(width, height);
    }

    void Update(float deltaTime) {
        if (m_running) {
            m_progress->SetValue(m_progress->GetValue() + deltaTime * m_speed->GetValue());
            if (m_progress->GetValue() >= 100.0f) SetRunning(false);
        }
    }

    bool SmokeTest() {
        using namespace LambUI;
        LAMBUI_LOGT(TAG, "SmokeTest");
        bool passed = true;
        m_manager.ClosePopup();
        m_manager.Update(0.0f);
        const auto tabs = m_tabs->GetComputedRect();
        Click(tabs.x + tabs.width * 0.375f, tabs.y + 12.0f);
        passed &= m_tabs->GetSelectedIndex() == 1;
        SetRunning(true);
        ClickCenter(*m_toggle);
        passed &= !m_running;
        ClickCenter(*m_reset);
        Update(1.0f);
        passed &= m_running && std::abs(m_progress->GetValue() - 8.0f) < 0.01f;
        Click(tabs.x + tabs.width * 0.125f, tabs.y + 12.0f);
        const auto tree = m_tree->GetComputedRect();
        Click(tree.x + 5.0f, tree.y + 12.0f);
        passed &= !m_tree->IsExpanded(m_folders.front());
        Click(tree.x + 5.0f, tree.y + 12.0f);
        Click(tree.x + 60.0f, tree.y + 36.0f);
        passed &= m_tree->GetSelectedNode() == 2;
        m_manager.InjectMouseWheel(0.0f, -48.0f);
        passed &= m_tree->GetScrollY() > 0.0f;
        m_tree->SetScrollOffset(0.0f, 0.0f);
        passed = SmokeScrollbars(*m_tree) && passed;
        Click(tree.x + 60.0f, tree.y + 36.0f, MouseButton::Right);
        passed &= m_context->IsOpen();
        const auto popup = m_context->GetComputedRect();
        Click(popup.x + 20.0f, popup.y + 12.0f);
        passed &= !m_context->IsOpen() && m_tabs->GetSelectedIndex() == 1;
        const auto menu = m_menuBar->GetComputedRect();
        Click(menu.x + 30.0f, menu.y + 12.0f);
        passed &= m_menuBar->GetMenu(0)->IsOpen();
        m_manager.InjectKeyEvent(ScanCode::Escape, true);
        passed &= m_manager.GetActivePopup() == nullptr;
        passed = SmokeGallery() && passed;
        Click(menu.x + 30.0f, menu.y + 12.0f);
        m_manager.InjectKeyEvent(ScanCode::Escape, true);
        passed &= m_manager.GetFocusedWidget() == m_menuBar;
        m_manager.InjectKeyEvent(ScanCode::Down, true);
        passed &= m_menuBar->GetMenu(0)->IsOpen();
        m_manager.InjectKeyEvent(ScanCode::Right, true);
        passed &= m_menuBar->GetMenu(1)->IsOpen();
        m_manager.InjectKeyEvent(ScanCode::Escape, true);
        m_manager.InjectKeyEvent(ScanCode::Tab, true);
        m_manager.InjectKeyEvent(ScanCode::Tab, false);
        m_manager.InjectKeyEvent(ScanCode::LeftShift, true);
        m_manager.InjectKeyEvent(ScanCode::Tab, true);
        m_manager.InjectKeyEvent(ScanCode::Tab, false);
        m_manager.InjectKeyEvent(ScanCode::LeftShift, false);
        passed &= m_manager.GetFocusedWidget() == m_menuBar;
        Click(tabs.x + tabs.width * 0.125f, tabs.y + 12.0f);
        m_manager.InjectKeyEvent(ScanCode::Right, true);
        passed &= m_tabs->GetSelectedIndex() == 1;
        m_tabs->SetSelectedIndex(0);
        m_progress->SetValue(35.0f);
        m_manager.InjectMouseMove(0.0f, 0.0f);
        m_manager.Update(0.0f);
        LAMBUI_LOGI(TAG, "Compound widgets: {}", passed ? "PASS" : "FAIL");
        return passed;
    }

    bool CaptureViews(const std::function<bool(const char*)>& capture) {
        LAMBUI_LOGT(TAG, "CaptureViews");
        m_manager.ClosePopup();
        m_tabs->SetSelectedIndex(0);
        m_manager.Update(0.0f);
        const auto rect = m_tree->GetComputedRect();
        Click(rect.x + rect.width - 24.0f, rect.y + rect.height - 24.0f, LambUI::MouseButton::Right);
        bool passed = m_context->IsOpen();
        passed = capture("menu") && passed;
        m_manager.InjectKeyEvent(LambUI::ScanCode::Escape, true);
        m_tabs->SetSelectedIndex(1);
        m_manager.Update(0.0f);
        passed = capture("job") && passed;
        m_tabs->SetSelectedIndex(2);
        m_controls->SetScrollOffset(0, 0);
        m_manager.Update(0);
        passed = capture("controls") && passed;
        m_controls->SetScrollOffset(0, 176);
        m_manager.Update(0);
        m_quality->Toggle();
        m_manager.Update(0);
        passed = capture("inputs") && passed;
        m_quality->Toggle();
        m_controls->SetScrollOffset(0, 1000);
        m_manager.Update(0);
        passed = capture("canvas") && passed;
        m_tabs->SetSelectedIndex(3);
        m_manager.Update(0);
        passed = capture("window-settings") && passed;
        m_settings->SetScrollOffset(0, 1000);
        m_manager.Update(0);
        passed = capture("window-settings-lower") && passed;
        m_settings->SetScrollOffset(0, 0);
        OpenWindow();
        m_manager.Update(0);
        passed = capture("window") && passed;
        m_window->Minimize();
        m_manager.Update(0);
        passed = capture("minimized") && passed;
        m_window->Maximize();
        m_manager.Update(0);
        passed = capture("maximized") && passed;
        m_window->Restore();
        m_window->Close();
        ShowTooltipForCapture();
        passed = capture("tooltip") && passed;
        m_manager.InjectMouseMove(0.0f, 0.0f);
        m_manager.Update(0.0f);
        return passed;
    }

private:
    void BuildGallery(LambUI::UICustomRenderCallback preview) {
        using namespace LambUI;
        LAMBUI_LOGT(TAG, "BuildGallery");
        auto* page = m_tabs->AddTab("Controls");
        m_controls = page->CreateChild<UIScrollContainer>("ControlGallery");
        m_controls->SetAllPoints(page);
        m_controls->SetContentSize(280, 510);
        auto* content = m_controls->GetContent();
        m_autoBuild = content->CreateChild<UICheckBox>("AutoBuild");
        m_autoBuild->SetText("Auto build");
        m_autoBuild->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 12);
        m_autoBuild->SetChecked(true);
        m_autoBuild->RegisterCallback(UIEventType::OnValueChanged, [this](const UIEventData&) {
            SetRunning(m_autoBuild->IsChecked());
        });
        m_draft = content->CreateChild<UIRadioButton>("Draft");
        m_final = content->CreateChild<UIRadioButton>("Final");
        m_draft->SetText("Draft");
        m_final->SetText("Final");
        m_draft->SetSize(116, 24);
        m_final->SetSize(116, 24);
        m_draft->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 46);
        m_final->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 140, 46);
        m_draft->SetChecked(true);
        auto* offline = content->CreateChild<UICheckBox>("Offline");
        offline->SetText("Cloud sync");
        offline->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 80);
        offline->SetEnabled(false);
        offline->SetTooltip("Cloud service unavailable");
        AddLabel(*content, "SpeedLabel", "Build speed", 12, 112);
        m_speed = content->CreateChild<UISlider>("BuildSpeed");
        m_speed->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 142);
        m_speed->SetSize(248, 20);
        m_speed->SetMinMaxValues(1, 20);
        m_speed->SetValue(8);
        AddLabel(*content, "NameLabel", "Asset name", 12, 180);
        m_name = content->CreateChild<UIInputBox>("AssetName");
        m_name->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 206);
        m_name->SetSize(248, 30);
        m_name->SetText("Contour");
        m_name->RegisterCallback(UIEventType::OnTextChanged, [this](const UIEventData&) {
            m_window->SetTitle(m_name->GetText());
        });
        AddLabel(*content, "QualityLabel", "Preview quality", 12, 254);
        m_quality = content->CreateChild<UIDropDownBox>("PreviewQuality");
        m_quality->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 280);
        m_quality->SetSize(248, 28);
        m_quality->SetOptions({"Balanced", "High", "Ultra"});
        AddLabel(*content, "CanvasLabel", "Material preview", 12, 386);
        auto* canvas = content->CreateChild<UICanvasWidget>("MaterialCanvas");
        canvas->SetPoint(AnchorPoint::TopLeft, content, AnchorPoint::TopLeft, 12, 412);
        canvas->SetSize(248, 86);
        canvas->SetRenderCallback(preview);
        canvas->SetMouseEnabled(false);

        m_window = m_manager.GetRoot().CreateChild<UIWindow>("PreviewWindow");
        m_window->SetTitle("Contour");
        m_window->SetSizeLimits(260, 180, 900, 700);
        m_window->SetVisible(false);
        auto* windowContent = m_window->GetContent();
        AddLabel(*windowContent, "MaterialName", "Material / Contour", 12, 8);
        auto* windowCanvas = windowContent->CreateChild<UICanvasWidget>("WindowCanvas");
        windowCanvas->SetPoint(AnchorPoint::TopLeft, windowContent, AnchorPoint::TopLeft, 12, 40);
        windowCanvas->SetPoint(AnchorPoint::BottomRight, windowContent, AnchorPoint::BottomRight, -12, -12);
        windowCanvas->SetRenderCallback(std::move(preview));
        windowCanvas->SetMouseEnabled(false);

        auto* settings = m_tabs->AddTab("Window");
        m_settings = settings->CreateChild<UIScrollContainer>("WindowSettings");
        m_settings->SetAllPoints(settings);
        m_settings->SetContentSize(280, 360);
        auto* options = m_settings->GetContent();
        m_openWindow = options->CreateChild<UIButton>("OpenPreview");
        m_openWindow->SetPoint(AnchorPoint::TopLeft, options, AnchorPoint::TopLeft, 12, 10);
        m_openWindow->SetSize(248, 30);
        m_openWindow->SetNormalColor(0x286550FFu);
        m_openWindow->SetHoverColor(0x347F66FFu);
        m_openWindow->SetPressedColor(0x1B493BFFu);
        AddLabel(*m_openWindow, "OpenLabel", "Open preview", 10, 3);
        m_openWindow->RegisterCallback(UIEventType::OnClick, [this](const UIEventData& event) {
            if (event.button == MouseButton::Left) OpenWindow();
        });
        for (bool moving : {true, false}) {
            auto* toggle = options->CreateChild<UICheckBox>(moving ? "MoveWindow" : "ResizeWindow");
            m_geometryToggles.push_back(toggle);
            toggle->SetText(moving ? "Movable" : "Resizable");
            toggle->SetPoint(AnchorPoint::TopLeft, options, AnchorPoint::TopLeft, 12, moving ? 52.0f : 84.0f);
            toggle->SetChecked(true);
            toggle->RegisterCallback(UIEventType::OnValueChanged, [this, toggle, moving](const UIEventData&) {
                if (moving) m_window->SetMovable(toggle->IsChecked());
                else m_window->SetResizable(toggle->IsChecked());
            });
        }
        for (int buttonIndex = 0; buttonIndex < 3; ++buttonIndex) {
            const auto button = static_cast<WindowButton>(buttonIndex);
            const float top = 124.0f + buttonIndex * 76.0f;
            AddLabel(*options, ToString(button), ToString(button), 12, top);
            for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
                const auto mode = static_cast<WindowButtonMode>(modeIndex);
                auto* radio = options->CreateChild<UIRadioButton>();
                m_buttonPolicies.push_back(radio);
                radio->SetGroup(ToString(button));
                radio->SetText(modeIndex == 0 ? "On" : modeIndex == 1 ? "Off" : "Hide");
                radio->SetTooltip(ToString(mode));
                radio->SetPoint(AnchorPoint::TopLeft, options, AnchorPoint::TopLeft, 12.0f + modeIndex * 84.0f, top + 26);
                radio->SetSize(82, 24);
                radio->SetChecked(modeIndex == 0);
                radio->RegisterCallback(UIEventType::OnValueChanged, [this, radio, button, mode](const UIEventData&) {
                    if (radio->IsChecked()) m_window->SetButtonMode(button, mode);
                });
            }
        }
    }

    void OpenWindow() {
        LAMBUI_LOGT(TAG, "OpenWindow");
        const auto root = m_manager.GetRoot().GetComputedRect();
        m_window->SetBounds(std::max(0.0f, (root.width - 300) * 0.5f), 140, 300, 260);
        m_window->SetVisible(true);
        m_window->BringToFront();
    }

    bool SmokeScrollbars(LambUI::UIScrollContainer& scroll) {
        using namespace LambUI;
        LAMBUI_LOGT(TAG, "SmokeScrollbars('{}')", scroll.GetName());
        scroll.SetScrollOffset(0.0f, 0.0f);
        m_manager.Update(0.0f);
        const auto rect = scroll.GetComputedRect();
        const auto content = scroll.GetContent()->GetComputedRect();
        const float overflow = std::max(0.0f, content.height - rect.height);
        if (overflow == 0.0f) return true;
        const float trackLength = rect.height - (content.width > rect.width ? 12.0f : 0.0f);
        const float thumbLength = std::max(20.0f, trackLength * rect.height / content.height);
        const float pointerX = rect.x + rect.width - 6.0f;
        m_manager.InjectMouseMove(pointerX, rect.y + 5.0f);
        m_manager.InjectMouseButton(MouseButton::Left, true);
        m_manager.InjectMouseMove(pointerX, rect.y + 5.0f + (trackLength - thumbLength) * 0.5f);
        m_manager.Update(0.0f);
        bool passed = std::abs(scroll.GetScrollY() - overflow * 0.5f) < 0.01f;
        m_manager.InjectMouseMove(pointerX, rect.y + rect.height + 40.0f);
        m_manager.InjectMouseButton(MouseButton::Left, false);
        passed &= std::abs(scroll.GetScrollY() - overflow) < 0.01f;
        Click(pointerX, rect.y + 2.0f);
        passed &= std::abs(scroll.GetScrollY() - std::max(0.0f, overflow - rect.height)) < 0.01f;
        scroll.SetScrollOffset(0.0f, 0.0f);
        m_manager.Update(0.0f);
        LAMBUI_LOGI(TAG, "Scrollbars '{}': {}", scroll.GetName(), passed ? "PASS" : "FAIL");
        return passed;
    }

    bool SmokeGallery() {
        using namespace LambUI;
        LAMBUI_LOGT(TAG, "SmokeGallery");
        m_tabs->SetSelectedIndex(2);
        m_controls->SetScrollOffset(0, 0);
        m_manager.Update(0);
        bool passed = SmokeScrollbars(*m_controls);
        ClickCenter(*m_autoBuild);
        passed &= !m_autoBuild->IsChecked() && !m_running;
        m_manager.InjectKeyEvent(ScanCode::Space, true);
        m_manager.InjectKeyEvent(ScanCode::Space, false);
        passed &= m_autoBuild->IsChecked() && m_running;
        ClickCenter(*m_final);
        passed &= m_final->IsChecked() && !m_draft->IsChecked();
        m_controls->SetScrollOffset(0, 180);
        m_manager.Update(0);
        ClickCenter(*m_name);
        m_manager.InjectCharacter(U'!');
        passed &= m_name->GetText() == "Contour!";
        m_manager.InjectKeyEvent(ScanCode::Backspace, true);
        ClickCenter(*m_quality);
        m_manager.Update(0);
        const auto dropdown = m_quality->GetComputedRect();
        Click(dropdown.x + 20, dropdown.y + dropdown.height + 36);
        passed &= m_quality->GetSelectedIndex() == 1 && !m_quality->IsExpanded();
        m_tabs->SetSelectedIndex(3);
        m_settings->SetScrollOffset(0, 0);
        m_manager.Update(0);
        passed = SmokeScrollbars(*m_settings) && passed;
        ClickCenter(*m_geometryToggles[0]);
        ClickCenter(*m_geometryToggles[1]);
        passed &= !m_window->IsMovable() && !m_window->IsResizable();
        ClickCenter(*m_geometryToggles[0]);
        ClickCenter(*m_geometryToggles[1]);
        for (size_t buttonIndex = 0; buttonIndex < 3; ++buttonIndex) {
            m_settings->SetScrollOffset(0, static_cast<float>(buttonIndex) * 76);
            m_manager.Update(0);
            for (size_t modeIndex : {1u, 2u, 0u}) {
                ClickCenter(*m_buttonPolicies[buttonIndex * 3 + modeIndex]);
                passed &= m_window->GetButtonMode(static_cast<WindowButton>(buttonIndex)) == static_cast<WindowButtonMode>(modeIndex);
            }
        }
        m_settings->SetScrollOffset(0, 0);
        m_manager.Update(0);
        ClickCenter(*m_openWindow);
        m_manager.Update(0);
        passed &= m_window->IsVisible();
        const auto bounds = m_window->GetComputedRect();
        m_manager.InjectMouseMove(bounds.x + 40, bounds.y + 16);
        m_manager.InjectMouseButton(MouseButton::Left, true);
        m_manager.InjectMouseMove(bounds.x + 50, bounds.y + 26);
        m_manager.Update(0);
        m_manager.InjectMouseButton(MouseButton::Left, false);
        passed &= std::abs(m_window->GetComputedRect().x - bounds.x - 10) < 0.01f;
        const auto moved = m_window->GetComputedRect();
        m_manager.InjectMouseMove(moved.x + moved.width - 2, moved.y + moved.height - 2);
        m_manager.InjectMouseButton(MouseButton::Left, true);
        m_manager.InjectMouseMove(moved.x + moved.width + 18, moved.y + moved.height + 18);
        m_manager.Update(0);
        m_manager.InjectMouseButton(MouseButton::Left, false);
        passed &= std::abs(m_window->GetComputedRect().width - moved.width - 20) < 0.01f;
        for (auto button : {WindowButton::Minimize, WindowButton::Minimize, WindowButton::Maximize, WindowButton::Maximize, WindowButton::Close}) {
            const auto rect = m_window->GetButtonRect(button);
            Click(rect.x + 12, rect.y + 12);
            m_manager.Update(0);
        }
        passed &= !m_window->IsVisible() && m_window->GetWindowState() == WindowState::Normal;
        m_draft->SetChecked(true);
        m_speed->SetValue(8);
        m_quality->SetSelectedIndex(0);
        m_controls->SetScrollOffset(0, 0);
        LAMBUI_LOGI(TAG, "Control/window gallery: {}", passed ? "PASS" : "FAIL");
        return passed;
    }

    void ShowTooltipForCapture() {
        LAMBUI_LOGT(TAG, "ShowTooltipForCapture");
        m_manager.ClosePopup();
        m_tabs->SetSelectedIndex(0);
        m_manager.Update(0.0f);
        const auto rect = m_tree->GetComputedRect();
        m_manager.InjectMouseMove(rect.x + 60.0f, rect.y + 36.0f);
        m_manager.Update(0.6f);
    }

    static constexpr const char* TAG = "WidgetShowcase";
    LambUI::UITextWidget* AddLabel(LambUI::UIWidget& parent, const char* name, const char* text, float x, float y) {
        LAMBUI_LOGT(TAG, "AddLabel('{}')", name);
        auto* label = parent.CreateChild<LambUI::UITextWidget>(name);
        label->SetMouseEnabled(false);
        label->SetTextMeasurer(m_manager.GetTextMeasurer());
        label->SetText(text);
        label->SetPoint(LambUI::AnchorPoint::TopLeft, &parent, LambUI::AnchorPoint::TopLeft, x, y);
        return label;
    }

    void SetRunning(bool running) {
        LAMBUI_LOGT(TAG, "SetRunning({})", running);
        m_running = running;
        m_toggleText->SetText(running ? "Pause" : "Resume");
        RefreshStatus();
    }

    void Restart() {
        LAMBUI_LOGT(TAG, "Restart");
        m_progress->SetValue(0.0f);
        SetRunning(true);
    }

    void RefreshStatus() {
        const int percent = static_cast<int>(m_progress->GetValue());
        const std::string text = (percent == 100 ? "Complete: " : m_running ? "Building: " : "Paused: ") + std::to_string(percent) + "%";
        if (m_status->GetText() == text) return;
        LAMBUI_LOGT(TAG, "Status('{}')", text);
        m_status->SetText(text);
    }

    void SetExpanded(bool expanded) {
        LAMBUI_LOGT(TAG, "SetExpanded({})", expanded);
        for (const auto folder : m_folders) m_tree->SetExpanded(folder, expanded);
    }

    void Click(float x, float y, LambUI::MouseButton button = LambUI::MouseButton::Left) {
        LAMBUI_LOGT(TAG, "Click({}, {}, {})", x, y, ToString(button));
        m_manager.Update(0.0f);
        m_manager.InjectMouseMove(x, y);
        m_manager.InjectMouseButton(button, true);
        m_manager.InjectMouseButton(button, false);
    }

    void ClickCenter(LambUI::UIWidget& widget) {
        LAMBUI_LOGT(TAG, "ClickCenter('{}')", widget.GetName());
        const auto rect = widget.GetComputedRect();
        Click(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
    }

    LambUI::UIManager& m_manager;
    LambUI::UIWidget* m_panel = nullptr;
    LambUI::UIMenuBar* m_menuBar = nullptr;
    LambUI::UITabControl* m_tabs = nullptr;
    LambUI::UITreeView* m_tree = nullptr;
    LambUI::UIContextMenu* m_context = nullptr;
    LambUI::UIProgressBar* m_progress = nullptr;
    LambUI::UITextWidget* m_status = nullptr;
    LambUI::UITextWidget* m_selection = nullptr;
    LambUI::UIButton* m_toggle = nullptr;
    LambUI::UIButton* m_reset = nullptr;
    LambUI::UITextWidget* m_toggleText = nullptr;
    LambUI::UIScrollContainer* m_controls = nullptr;
    LambUI::UIScrollContainer* m_settings = nullptr;
    LambUI::UICheckBox* m_autoBuild = nullptr;
    LambUI::UIRadioButton* m_draft = nullptr;
    LambUI::UIRadioButton* m_final = nullptr;
    LambUI::UISlider* m_speed = nullptr;
    LambUI::UIInputBox* m_name = nullptr;
    LambUI::UIDropDownBox* m_quality = nullptr;
    LambUI::UIWindow* m_window = nullptr;
    LambUI::UIButton* m_openWindow = nullptr;
    std::vector<LambUI::UICheckBox*> m_geometryToggles;
    std::vector<LambUI::UIRadioButton*> m_buttonPolicies;
    std::vector<LambUI::UITreeView::NodeId> m_folders;
    std::vector<std::string> m_names;
    bool m_running = true;
};

} // namespace LambUIExamples