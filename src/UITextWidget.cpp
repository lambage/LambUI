#include "LambUI/UITextWidget.h"
#include "LambUI/IRenderer.h"
#include "UITextLayout.h"

namespace LambUI {

namespace {
constexpr const char* TAG = "UITextWidget";
} // namespace

UITextWidget::UITextWidget(std::string name) : UIWidget(std::move(name)) {
    LAMBUI_LOGT(TAG, "'{}' constructed", GetName());
}

void UITextWidget::SetText(const std::string& text) {
    LAMBUI_LOGT(TAG, "'{}' SetText('{}')", GetName(), text);
    m_text = text;
    m_selectionAnchor = m_cursor = 0;
    m_draggingSelection = false;
    m_cursorAtLineEnd = false;
    m_preferredCaretX = -1.0f;
    UpdateTextSize();
}

void UITextWidget::SetFont(void* fontHandle) {
    LAMBUI_LOGT(TAG, "'{}' SetFont({})", GetName(), fmt::ptr(fontHandle));
    m_fontHandle = fontHandle;
    UpdateTextSize();
}

void UITextWidget::SetTextMeasurer(const ITextMeasurer* measurer) {
    LAMBUI_LOGT(TAG, "'{}' SetTextMeasurer({})", GetName(), fmt::ptr(measurer));
    m_textMeasurer = measurer;
    UpdateTextSize();
}

void UITextWidget::UpdateTextSize() {
    LAMBUI_LOGT(TAG, "'{}' UpdateTextSize", GetName());
    m_cursorAtLineEnd = false;
    m_preferredCaretX = -1.0f;
    if (m_textMeasurer && !m_wordWrap) {
        const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, 0, false);
        float width = 0.0f;
        for (const auto& line : layout.lines) width = std::max(width, line.width);
        SetSize(width, layout.lineHeight * static_cast<float>(layout.lines.size()));
    }
    MarkDirty();
}

void UITextWidget::SetWordWrap(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetWordWrap({})", GetName(), enabled);
    m_wordWrap = enabled;
    UpdateTextSize();
}

void UITextWidget::SetSelectionEnabled(bool enabled) {
    LAMBUI_LOGT(TAG, "'{}' SetSelectionEnabled({})", GetName(), enabled);
    m_selectionEnabled = enabled;
    if (!enabled) {
        m_draggingSelection = false;
        ClearSelection();
    }
    MarkDirty();
}

size_t UITextWidget::GetSelectionStart() const { return std::min(m_selectionAnchor, m_cursor); }
size_t UITextWidget::GetSelectionEnd() const { return std::max(m_selectionAnchor, m_cursor); }

std::string UITextWidget::GetSelectedText() const {
    return m_text.substr(GetSelectionStart(), GetSelectionEnd() - GetSelectionStart());
}

void UITextWidget::SetSelection(size_t anchor, size_t cursor) {
    LAMBUI_LOGT(TAG, "'{}' SetSelection({}, {})", GetName(), anchor, cursor);
    if (!m_selectionEnabled) return;
    const auto boundary = [&](size_t position) {
        position = std::min(position, m_text.size());
        while (position > 0 && position < m_text.size() &&
               (static_cast<unsigned char>(m_text[position]) & 0xC0) == 0x80) --position;
        return position;
    };
    m_selectionAnchor = boundary(anchor);
    m_cursor = boundary(cursor);
    m_cursorAtLineEnd = false;
    m_preferredCaretX = -1.0f;
    MarkDirty();
}

void UITextWidget::SelectAll() {
    LAMBUI_LOGT(TAG, "'{}' SelectAll", GetName());
    SetSelection(0, m_text.size());
}

void UITextWidget::ClearSelection() {
    LAMBUI_LOGT(TAG, "'{}' ClearSelection", GetName());
    m_selectionAnchor = m_cursor;
    MarkDirty();
}

void UITextWidget::UpdateModifiers(bool shift, bool control) {
    if (m_shiftDown == shift && m_controlDown == control) return;
    LAMBUI_LOGT(TAG, "'{}' UpdateModifiers(shift={}, control={})", GetName(), shift, control);
    m_shiftDown = shift;
    m_controlDown = control;
}

void UITextWidget::OnFocusGained() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusGained", GetName());
    m_isFocused = true;
    MarkDirty();
}

void UITextWidget::OnFocusLost() {
    LAMBUI_LOGT(TAG, "'{}' OnFocusLost", GetName());
    m_isFocused = false;
    m_draggingSelection = false;
    UpdateModifiers(false, false);
    MarkDirty();
}

void UITextWidget::PositionCursor(float mouseX, float mouseY, bool extend) {
    if (!std::isfinite(mouseX) || !std::isfinite(mouseY)) return;
    LAMBUI_LOGT(TAG, "'{}' PositionCursor({}, {}, extend={})", GetName(), mouseX, mouseY, extend);
    const auto rect = GetContentRect();
    const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, rect.width, m_wordWrap);
    const float row = std::floor((mouseY - rect.y) / layout.lineHeight);
    const size_t index = static_cast<size_t>(boost::algorithm::clamp(row, 0.0f, static_cast<float>(layout.lines.size() - 1)));
    const auto& line = layout.lines[index];
    m_cursor = TextLayout::HitPosition(m_text, line, mouseX - rect.x, m_textMeasurer, m_fontHandle);
    m_cursorAtLineEnd = m_cursor == line.end;
    if (!extend) m_selectionAnchor = m_cursor;
    m_preferredCaretX = -1.0f;
    MarkDirty();
}

void UITextWidget::OnEvent(const UIEventData& data) {
    LAMBUI_LOGT(TAG, "'{}' OnEvent({})", GetName(), ToString(data.type));
    if (!m_selectionEnabled || data.button != MouseButton::Left) return;
    if (data.type == UIEventType::OnMouseDown) {
        if (!m_isFocused) return;
        PositionCursor(data.mouseX, data.mouseY, m_shiftDown);
        m_draggingSelection = true;
        data.handled = true;
    } else if (data.type == UIEventType::OnMouseUp) {
        m_draggingSelection = false;
        data.handled = true;
    } else if (data.type == UIEventType::OnClick) data.handled = true;
}

void UITextWidget::OnDrag(float mouseX, float mouseY) {
    if (!m_isFocused || !m_selectionEnabled || !m_draggingSelection) return;
    LAMBUI_LOGT(TAG, "'{}' OnDrag({}, {})", GetName(), mouseX, mouseY);
    PositionCursor(mouseX, mouseY, true);
}

void UITextWidget::OnKeyEvent(uint32_t scanCode, bool isDown) {
    LAMBUI_LOGT(TAG, "'{}' OnKeyEvent({}, {})", GetName(), scanCode, isDown);
    if (!isDown || !m_isFocused || !m_selectionEnabled) return;
    if (m_controlDown && scanCode == ScanCode::A) { SelectAll(); return; }
    if (scanCode != ScanCode::Left && scanCode != ScanCode::Right &&
        scanCode != ScanCode::Up && scanCode != ScanCode::Down &&
        scanCode != ScanCode::Home && scanCode != ScanCode::End) return;
    const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, GetContentRect().width, m_wordWrap);
    const size_t index = TextLayout::CursorLine(layout, m_cursor, m_cursorAtLineEnd);
    const auto& line = layout.lines[index];
    if (!m_shiftDown && GetSelectionStart() != GetSelectionEnd() &&
        (scanCode == ScanCode::Left || scanCode == ScanCode::Right)) {
        m_cursor = scanCode == ScanCode::Left ? GetSelectionStart() : GetSelectionEnd();
        m_cursorAtLineEnd = false;
        m_preferredCaretX = -1.0f;
    } else if (scanCode == ScanCode::Up || scanCode == ScanCode::Down) {
        if (m_preferredCaretX < 0.0f)
            m_preferredCaretX = TextLayout::Width(m_text, line.begin, m_cursor, m_textMeasurer, m_fontHandle);
        const size_t target = scanCode == ScanCode::Up ? (index == 0 ? 0 : index - 1)
            : std::min(index + 1, layout.lines.size() - 1);
        const auto& destination = layout.lines[target];
        m_cursor = TextLayout::HitPosition(m_text, destination, m_preferredCaretX, m_textMeasurer, m_fontHandle);
        m_cursorAtLineEnd = m_cursor == destination.end;
    } else {
        m_preferredCaretX = -1.0f;
        m_cursorAtLineEnd = false;
        if (scanCode == ScanCode::Home) m_cursor = m_controlDown ? 0 : line.begin;
        else if (scanCode == ScanCode::End) {
            m_cursor = m_controlDown ? m_text.size() : line.end;
            m_cursorAtLineEnd = true;
        } else if (scanCode == ScanCode::Right) m_cursor = TextLayout::Next(m_text, m_cursor);
        else if (scanCode == ScanCode::Left && m_cursor > 0) {
            do { --m_cursor; } while (m_cursor > 0 && (static_cast<unsigned char>(m_text[m_cursor]) & 0xC0) == 0x80);
        }
    }
    if (!m_shiftDown) m_selectionAnchor = m_cursor;
    MarkDirty();
}

void UITextWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const UIRect rect = GetContentRect();
    const auto layout = TextLayout::Build(m_text, m_textMeasurer, m_fontHandle, rect.width, m_wordWrap);
    if (m_wordWrap || m_selectionEnabled) {
        UIRenderCommand clip;
        clip.type = RenderCommandType::PushScissor;
        clip.x = rect.x;
        clip.y = rect.y;
        clip.width = rect.width;
        clip.height = rect.height;
        bucket.push_back(clip);
    }
    for (size_t index = 0; index < layout.lines.size(); ++index) {
        const auto& line = layout.lines[index];
        const size_t begin = std::max(GetSelectionStart(), line.begin);
        const size_t end = std::min(GetSelectionEnd(), line.end);
        const bool newline = index + 1 < layout.lines.size() && layout.lines[index + 1].begin > line.end &&
            GetSelectionStart() <= line.end && GetSelectionEnd() > line.end;
        if (m_selectionEnabled && (begin < end || newline)) {
            const float left = TextLayout::Width(m_text, line.begin, std::min(begin, line.end), m_textMeasurer, m_fontHandle);
            const float right = newline ? std::max(left + 1.0f, line.width + 4.0f) :
                TextLayout::Width(m_text, line.begin, end, m_textMeasurer, m_fontHandle);
            UIRenderCommand highlight;
            highlight.type = RenderCommandType::DrawQuad;
            highlight.x = rect.x + left;
            highlight.y = rect.y + static_cast<float>(index) * layout.lineHeight;
            highlight.width = std::max(0.0f, right - left);
            highlight.height = layout.lineHeight;
            highlight.color = m_isFocused ? 0x287EA8FFu : 0x445A66FFu;
            bucket.push_back(highlight);
        }
        UIRenderCommand cmd;
        cmd.type = RenderCommandType::DrawString;
        cmd.x = rect.x;
        cmd.y = rect.y + static_cast<float>(index) * layout.lineHeight;
        cmd.width = line.width;
        cmd.height = layout.lineHeight;
        cmd.color = m_color;
        cmd.fontHandle = m_fontHandle;
        cmd.text = m_text.substr(line.begin, line.end - line.begin);
        bucket.push_back(cmd);
    }
    if (m_wordWrap || m_selectionEnabled) {
        UIRenderCommand pop;
        pop.type = RenderCommandType::PopScissor;
        bucket.push_back(pop);
    }
}

} // namespace LambUI
