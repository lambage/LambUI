#pragma once

#include "lambui_export.h"
#include "UIControl.h"
#include <string>

namespace LambUI {

class ITextMeasurer;

// For player name fields, chat entry bars, or macro creation.
class LAMBUI_API UIInputBox : public UIControl, public IDraggable {
public:
    explicit UIInputBox(std::string name = {});

    bool IsFocused() const { return m_isFocused; }
    void AppendCharacter(char32_t codepoint);
    void Backspace();
    void SubmitEnter();
    bool PasteText(const std::string& text);

    const std::string& GetText() const { return m_text; }
    size_t GetCursorPosition() const { return m_cursor; }
    void SetText(const std::string& text);
    void SetFont(void* fontHandle);
    void* GetFont() const { return m_fontHandle; }
    void SetMultiline(bool enabled);
    bool IsMultiline() const { return m_multiline; }
    void SetWordWrap(bool enabled);
    bool IsWordWrapEnabled() const { return m_wordWrap; }
    void SetEditingEnabled(bool enabled);
    bool IsEditingEnabled() const { return m_editingEnabled; }
    void SetSelectionEnabled(bool enabled);
    bool IsSelectionEnabled() const { return m_selectionEnabled; }
    void SetSelection(size_t anchor, size_t cursor);
    void SelectAll();
    void ClearSelection();
    size_t GetSelectionStart() const;
    size_t GetSelectionEnd() const;
    std::string GetSelectedText() const;
    void OnDrag(float mouseX, float mouseY) override;

    // IFocusable
    bool CanUseDialogDefault() const override { return !m_multiline; }
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCharacter(char32_t codepoint) override { AppendCharacter(codepoint); }
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;

protected:
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    friend class UIManager;
    void UpdateCaret(float deltaTime, const ITextMeasurer* textMeasurer);
    void MoveCursor(uint32_t scanCode);
    void UpdateModifiers(bool shift, bool control);
    bool EraseSelection();
    void PositionCursor(float mouseX, float mouseY, bool extend);

    std::string m_text;
    size_t m_cursor = 0;
    void* m_fontHandle = nullptr;
    bool m_isFocused = false;
    float m_caretElapsed = 0.0f;
    const ITextMeasurer* m_textMeasurer = nullptr;
    bool m_multiline = false;
    bool m_wordWrap = true;
    bool m_cursorAtLineEnd = false;
    float m_preferredCaretX = -1.0f;
    size_t m_selectionAnchor = 0;
    bool m_selectionEnabled = true;
    bool m_editingEnabled = true;
    bool m_shiftDown = false;
    bool m_controlDown = false;
    bool m_draggingSelection = false;
    float m_dragX = 0.0f;
    float m_dragY = 0.0f;
    float m_dragElapsed = 0.0f;
    float m_scrollX = 0.0f;
    float m_scrollY = 0.0f;
};

} // namespace LambUI
