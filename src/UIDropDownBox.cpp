#include "LambUI/UIDropDownBox.h"
#include "LambUI/UITextWidget.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UIDropDownBox";
const std::string kEmptyOption;
constexpr float kOptionRowHeight = 24.0f;
} // namespace

UIDropDownBox::UIDropDownBox(std::string name) : UIControl(std::move(name)) {}

void UIDropDownBox::SetOptions(std::vector<std::string> options) {
    LAMBUI_LOGT(TAG, "'{}' SetOptions({} options)", GetName(), options.size());
    m_options = std::move(options);
    m_selectedIndex = m_options.empty() ? -1 : 0;
    m_isExpanded = false;
    RebuildOptionButtons();
}

void UIDropDownBox::SetSelectedIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_options.size())) return;
    LAMBUI_LOGT(TAG, "'{}' SetSelectedIndex({} -> {})", GetName(), m_selectedIndex, index);
    m_selectedIndex = index;
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
    m_isExpanded = !m_isExpanded;
    LAMBUI_LOGT(TAG, "'{}' Toggle -> expanded={}", GetName(), m_isExpanded);
    for (size_t index = 0; index < m_optionButtons.size(); ++index) {
        m_optionButtons[index]->SetVisible(m_isExpanded && index < m_options.size());
    }
    MarkDirty();
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
    for (auto* button : m_optionButtons) button->SetVisible(false);

    UIWidget* previous = nullptr;
    for (size_t optionIndex = 0; optionIndex < m_options.size(); ++optionIndex) {
        UIButton* option = nullptr;
        if (optionIndex < m_optionButtons.size()) {
            option = m_optionButtons[optionIndex];
        } else {
            option = CreateChild<UIButton>(GetName() + "_Option" + std::to_string(optionIndex));
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

    UIRenderCommand background;
    background.type = RenderCommandType::DrawQuad;
    background.x = rect.x;
    background.y = rect.y;
    background.width = rect.width;
    background.height = rect.height;
    background.color = 0x404040FFu;
    bucket.push_back(background);

    UIRenderCommand label;
    label.type = RenderCommandType::DrawString;
    label.x = rect.x + 4.0f;
    label.y = rect.y + 4.0f;
    label.color = 0xFFFFFFFFu;
    label.text = GetSelectedOption();
    bucket.push_back(label);
}

} // namespace LambUI
