#include "LambUI/UICanvasWidget.h"

namespace LambUI {

UICanvasWidget::UICanvasWidget(std::string name) : UIWidget(std::move(name)) {}

void UICanvasWidget::SetRenderCallback(UICustomRenderCallback callback, void* userData) {
    m_renderCallback = std::move(callback);
    m_userData = userData;
    MarkDirty();
}

void UICanvasWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    if (!m_renderCallback) return;

    const UIRect& rect = GetComputedRect();
    UIRenderCommand cmd;
    cmd.type = RenderCommandType::CustomCallback;
    cmd.x = rect.x;
    cmd.y = rect.y;
    cmd.width = rect.width;
    cmd.height = rect.height;
    cmd.customRenderFunc = m_renderCallback;
    cmd.customRenderUserData = m_userData;
    bucket.push_back(cmd);
}

} // namespace LambUI
