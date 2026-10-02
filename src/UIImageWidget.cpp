#include "LambUI/UIImageWidget.h"

#include <algorithm>
#include <utility>

namespace LambUI {

UIImageWidget::UIImageWidget(std::string name) : UIWidget(std::move(name)) {}

void UIImageWidget::SetImageLoader(UIImageLoader loader) {
    m_loader = std::move(loader);
    SetSource(m_source);
}

bool UIImageWidget::SetSource(const std::string& source) {
    m_source = source;
    m_image = {};
    MarkDirty();
    if (m_loader && !m_source.empty()) {
        const auto image = m_loader(m_source);
        if (image.textureHandle && image.width > 0 && image.height > 0)
            m_image = image;
    }
    return IsLoaded();
}

void UIImageWidget::OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) {
    const auto& rect = GetComputedRect();
    if (!IsLoaded() || rect.width <= 0 || rect.height <= 0) return;

    UIRenderCommand command;
    command.type = RenderCommandType::DrawQuad;
    command.x = rect.x;
    command.y = rect.y;
    command.width = rect.width;
    command.height = rect.height;
    command.u0 = command.v0 = 0;
    command.u1 = command.v1 = 1;
    command.color = m_tint;
    command.textureHandle = m_image.textureHandle;

    if (m_fit == ImageFit::Contain) {
        const float scale = std::min(rect.width / m_image.width, rect.height / m_image.height);
        command.width = m_image.width * scale;
        command.height = m_image.height * scale;
        command.x += (rect.width - command.width) * 0.5f;
        command.y += (rect.height - command.height) * 0.5f;
    } else if (m_fit == ImageFit::Cover) {
        const float scale = std::max(rect.width / m_image.width, rect.height / m_image.height);
        const float visibleWidth = rect.width / (m_image.width * scale);
        const float visibleHeight = rect.height / (m_image.height * scale);
        command.u0 = (1 - visibleWidth) * 0.5f;
        command.v0 = (1 - visibleHeight) * 0.5f;
        command.u1 = 1 - command.u0;
        command.v1 = 1 - command.v0;
    }
    bucket.push_back(command);
}

} // namespace LambUI