#pragma once

#include "lambui_export.h"
#include "UIWidget.h"
#include "UIInteractionInterfaces.h"
#include <cstdint>
#include <string>

namespace LambUI {

class ITextMeasurer;

// Renders a string via the backend's font/SDF pipeline; the library only
// stores the string and (optionally) requests metrics through ITextMeasurer.
class LAMBUI_API UITextWidget : public UIWidget, public IFocusable, public IDraggable {
public:
    explicit UITextWidget(std::string name = {});

    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }

    void SetFont(void* fontHandle);
    void* GetFont() const { return m_fontHandle; }
    void SetColor(uint32_t color) { m_color = color; MarkDirty(); }

    // Optional: unwrapped labels resize to fit the text on change.
    void SetTextMeasurer(const ITextMeasurer* measurer);

    void SetWordWrap(bool enabled);
    bool IsWordWrapEnabled() const { return m_wordWrap; }
    void SetSelectionEnabled(bool enabled);
    bool IsSelectionEnabled() const { return m_selectionEnabled; }
    void SetSelection(size_t anchor, size_t cursor);
    void SelectAll();
    void ClearSelection();
    size_t GetSelectionStart() const;
    size_t GetSelectionEnd() const;
    std::string GetSelectedText() const;
    bool CanFocus() const override { return m_selectionEnabled && IsMouseEnabled(); }
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnCharacter(char32_t) override {}
    void OnKeyEvent(uint32_t scanCode, bool isDown) override;
    void OnDrag(float mouseX, float mouseY) override;

protected:
    void OnEvent(const UIEventData& data) override;
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    friend class UIManager;
    void UpdateModifiers(bool shift, bool control);
    void PositionCursor(float mouseX, float mouseY, bool extend);
    void UpdateTextSize();

    std::string m_text;
    void* m_fontHandle = nullptr;
    uint32_t m_color = 0xFFFFFFFFu;
    const ITextMeasurer* m_textMeasurer = nullptr;
    bool m_wordWrap = false;
    bool m_selectionEnabled = false;
    bool m_isFocused = false;
    bool m_shiftDown = false;
    bool m_controlDown = false;
    bool m_draggingSelection = false;
    bool m_cursorAtLineEnd = false;
    float m_preferredCaretX = -1.0f;
    size_t m_selectionAnchor = 0;
    size_t m_cursor = 0;
};

} // namespace LambUI
