#include "LambUI/UIDropDownBox.h"
#include "LambUI/UITextWidget.h"
#include "LambUI/UIContextMenu.h"
#include "LambUI/UIManager.h"
#include <algorithm>

namespace LambUI {

namespace {
constexpr const char* TAG = "UIDropDownBox";
const std::string kEmptyOption;
constexpr float kOptionRowHeight = 24.0f;
} // namespace

UIDropDownBox::UIDropDownBox(std::string name) : UIControl(std::move(name)) {}

UIDropDownBox::UIDropDownBox(UIManager& manager, std::string name)
    : UIControl(std::move(name)), m_manager(&manager) {}

bool UIDropDownBox::IsExpanded() const {
    return m_popup ? m_popup->IsOpen() : m_isExpanded;
}

void UIDropDownBox::SetOptions(std::vector<std::string> options) {
    LAMBUI_LOGT(TAG, "'{}' SetOptions({} options)", GetName(), options.size());
    if (m_popup) m_popup->Close();
    m_options = std::move(options);
    m_selectedIndex = m_options.empty() ? -1 : 0;
    m_isExpanded = false;
    RebuildOptionButtons();
}

void UIDropDownBox::SetFont(void* fontHandle) {
    m_fontHandle = fontHandle;
    if (m_popup) m_popup->SetFont(fontHandle);
    for (auto* label : m_optionLabels) label->SetFont(fontHandle);
    MarkDirty();
}

void UIDropDownBox::SetSelectedIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_options.size())) return;
    LAMBUI_LOGT(TAG, "'{}' SetSelectedIndex({} -> {})", GetName(), m_selectedIndex, index);
    m_selectedIndex = index;
    if (m_popup) m_popup->Close();
    m_isExpanded = false;
    for (UIButton* button : m_optionButtons) button->SetVisible(false);
    MarkDirty();
    FireEvent(UIEventData{UIEventType::OnValueChanged});
}

const std::string& UIDropDownBox::GetSelectedOption() const {
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_options.size())) {
        return kEmptyOption;
    }
    return m_options[static_cast<size_t>(m_selectedIndex)];
}

void UIDropDownBox::Toggle() {
    if (m_manager) {
        if (m_popup && m_popup->IsOpen()) {
            m_popup->Close();
        } else if (m_popup && !m_options.empty()) {
            const auto& rect = GetComputedRect();
            const auto& viewport = m_manager->GetRoot().GetComputedRect();
            const float below = std::max(0.0f, viewport.y + viewport.height - rect.y - rect.height);
            const float above = std::max(0.0f, rect.y - viewport.y);
            const float requestedHeight = static_cast<float>(m_options.size()) * kOptionRowHeight;
            const bool openAbove = requestedHeight > below && above > below;
            const float height = std::min(requestedHeight, openAbove ? above : below);
            m_popup->SetSize(rect.width, height);
            m_popup->SetScrollOffset(0.0f, 0.0f);
            m_manager->ShowPopup(*m_popup, rect.x, openAbove ? rect.y - height : rect.y + rect.height, this, true);
        }
        MarkDirty();
        return;
    }
    m_isExpanded = !m_isExpanded;
    LAMBUI_LOGT(TAG, "'{}' Toggle -> expanded={}", GetName(), m_isExpanded);
    for (size_t index = 0; index < m_optionButtons.size(); ++index) {
        m_optionButtons[index]->SetVisible(m_isExpanded && index < m_options.size());
    }
    MarkDirty();
}

void UIDropDownBox::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown) {
        UIControl::OnKeyEvent(scanCode, false);
        return;
    }
    if (scanCode == ScanCode::Escape) {
        if (IsExpanded()) Toggle();
    } else if (!m_options.empty() && (scanCode == ScanCode::Up || scanCode == ScanCode::Down ||
               scanCode == ScanCode::Home || scanCode == ScanCode::End)) {
        const int last = static_cast<int>(m_options.size()) - 1;
        int index = m_selectedIndex;
        if (scanCode == ScanCode::Up) index = std::max(0, index - 1);
        else if (scanCode == ScanCode::Down) index = std::min(last, index + 1);
        else index = scanCode == ScanCode::Home ? 0 : last;
        if (index != m_selectedIndex) SetSelectedIndex(index);
    } else UIControl::OnKeyEvent(scanCode, true);
}

void UIDropDownBox::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    UIControl::OnFocusLost();
    if (!m_manager && m_isExpanded) Toggle();
}

void UIDropDownBox::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    UIControl::OnEvent(data);
    if (data.type == UIEventType::OnClick) {
        data.handled = true;
        Toggle();
    }
}

void UIDropDownBox::RebuildOptionButtons() {
    LAMBUI_LOGT(TAG, "'{}' RebuildOptionButtons", GetName());
    if (m_manager) {
        if (!m_popup) m_popup = m_manager->GetOverlayRoot().CreateChild<UIContextMenu>(*m_manager, GetName() + "_Options");
        std::vector<UIMenuItem> items;
        for (size_t index = 0; index < m_options.size(); ++index) {
            items.push_back({m_options[index], [this, index]() { SetSelectedIndex(static_cast<int>(index)); }});
        }
        m_popup->SetItems(std::move(items));
        m_popup->SetFont(m_fontHandle);
        MarkDirty();
        return;
    }
    for (auto* button : m_optionButtons) button->SetVisible(false);

    UIWidget* previous = nullptr;
    for (size_t optionIndex = 0; optionIndex < m_options.size(); ++optionIndex) {
        UIButton* option = nullptr;
        if (optionIndex < m_optionButtons.size()) {
            option = m_optionButtons[optionIndex];
        } else {
            option = CreateChild<UIButton>(GetName() + "_Option" + std::to_string(optionIndex));
            option->SetKeyboardEnabled(false);
            option->SetNormalColor(0x404040FFu);
            option->SetHoverColor(0x606060FFu);
            option->SetPressedColor(0x303030FFu);
            auto* label = option->CreateChild<UITextWidget>();
            label->SetMouseEnabled(false);
            label->SetPoint(AnchorPoint::TopLeft, option, AnchorPoint::TopLeft, 4.0f, 2.0f);
            m_optionLabels.push_back(label);
            m_optionButtons.push_back(option);
        }
        m_optionLabels[optionIndex]->SetText(m_options[optionIndex]);
        m_optionLabels[optionIndex]->SetFont(m_fontHandle);
        option->ClearPoints();

        // Anchor to the previous option (or this box for the first one) so the
        // stack stays correct even if the box is resized or reflowed later.
        UIWidget* anchorSource = previous ? previous : static_cast<UIWidget*>(this);
        option->SetPoint(AnchorPoint::TopLeft, anchorSource, AnchorPoint::BottomLeft, 0.0f, 0.0f);
        option->SetPoint(AnchorPoint::TopRight, anchorSource, AnchorPoint::BottomRight, 0.0f, 0.0f);
        option->SetSize(0.0f, kOptionRowHeight);
        option->SetVisible(m_isExpanded);

        const int index = static_cast<int>(optionIndex);
        option->RegisterCallback(UIEventType::OnClick, [this, index](const UIEventData& data) {
            LAMBUI_LOGT(TAG, "'{}' option click index={}", GetName(), index);
            data.handled = true;
            SetSelectedIndex(index);
        });

        previous = option;
    }
}

void UIDropDownBox::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect& rect = GetComputedRect();
    if (rect.width <= 0 || rect.height <= 0) return;

    UIRenderCommand background;
    background.type = RenderCommandType::DrawQuad;
    background.x = rect.x;
    background.y = rect.y;
    background.width = rect.width;
    background.height = rect.height;
    background.color = GetState() == ControlState::Pressed ? 0x30383FFFu :
        (GetState() == ControlState::Hovered ? 0x4B555DFFu : 0x40484FFFu);
    bucket.push_back(background);

    const auto quad = [&](float x, float y, float width, float height, uint32_t color) {
        UIRenderCommand command;
        command.x = x;
        command.y = y;
        command.width = width;
        command.height = height;
        command.color = color;
        bucket.push_back(command);
    };
    const float stroke = std::min({1.0f, rect.width, rect.height});
    const float arrowWidth = std::min(28.0f, rect.width);
    const float arrowLeft = rect.x + rect.width - arrowWidth;
    const uint32_t outline = (HasKeyboardFocus() && AreFocusHighlightsVisible()) || IsExpanded() ? 0x67DBB3FFu :
        (GetState() == ControlState::Hovered ? 0xAAB8C2FFu : 0x71808AFFu);
    quad(arrowLeft, rect.y, arrowWidth, rect.height, 0x30383FFFu);
    quad(rect.x, rect.y, rect.width, stroke, outline);
    quad(rect.x, rect.y + rect.height - stroke, rect.width, stroke, outline);
    quad(rect.x, rect.y, stroke, rect.height, outline);
    quad(rect.x + rect.width - stroke, rect.y, stroke, rect.height, outline);
    quad(arrowLeft, rect.y, stroke, rect.height, outline);
    const float step = std::min({2.0f, arrowWidth / 8.0f, rect.height / 6.0f});
    const float centerX = arrowLeft + arrowWidth * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    for (int index = 0; index < 3; ++index) {
        const float arrowY = centerY + (IsExpanded() ? 0.5f - index : index - 1.5f) * step;
        quad(centerX + (index - 3) * step, arrowY, step, step, 0xE5EBEFFFu);
        quad(centerX + (2 - index) * step, arrowY, step, step, 0xE5EBEFFFu);
    }

    UIRenderCommand clip;
    clip.type = RenderCommandType::PushScissor;
    clip.x = rect.x + std::min(6.0f, rect.width - arrowWidth);
    clip.y = rect.y + stroke;
    clip.width = std::max(0.0f, arrowLeft - stroke - clip.x);
    clip.height = std::max(0.0f, rect.height - stroke * 2);
    bucket.push_back(clip);
    UIRenderCommand label;
    label.type = RenderCommandType::DrawString;
    label.x = clip.x;
    label.y = rect.y + 4.0f;
    label.color = 0xFFFFFFFFu;
    label.text = GetSelectedOption();
    label.fontHandle = m_fontHandle;
    bucket.push_back(label);
    clip.type = RenderCommandType::PopScissor;
    bucket.push_back(clip);
}

} // namespace LambUI
