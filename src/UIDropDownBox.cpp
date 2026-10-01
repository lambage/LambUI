#include "LambUI/UIDropDownBox.h"

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
    for (UIButton* button : m_optionButtons) {
        button->SetVisible(m_isExpanded);
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
    m_optionButtons.clear();

    UIWidget* previous = nullptr;
    for (size_t i = 0; i < m_options.size(); ++i) {
        UIButton* option = CreateChild<UIButton>(GetName() + "_Option" + std::to_string(i));

        // Anchor to the previous option (or this box for the first one) so the
        // stack stays correct even if the box is resized or reflowed later.
        UIWidget* anchorSource = previous ? previous : static_cast<UIWidget*>(this);
        option->SetPoint(AnchorPoint::TopLeft, anchorSource, AnchorPoint::BottomLeft, 0.0f, 0.0f);
        option->SetPoint(AnchorPoint::TopRight, anchorSource, AnchorPoint::BottomRight, 0.0f, 0.0f);
        option->SetSize(0.0f, kOptionRowHeight);
        option->SetVisible(m_isExpanded);

        const int index = static_cast<int>(i);
        option->RegisterCallback(UIEventType::OnClick, [this, index](const UIEventData& data) {
            LAMBUI_LOGT(TAG, "'{}' option click index={}", GetName(), index);
            data.handled = true;
            SetSelectedIndex(index);
        });

        m_optionButtons.push_back(option);
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
    background.color = 0xFF404040u;
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
