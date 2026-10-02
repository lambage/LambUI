#pragma once

#include "UIWidget.h"
#include <functional>

namespace LambUI {

struct UIImage {
    void* textureHandle = nullptr;
    int width = 0;
    int height = 0;
};

using UIImageLoader = std::function<UIImage(const std::string&)>;

enum class ImageFit { Contain, Cover, Stretch };

class LAMBUI_API UIImageWidget : public UIWidget {
public:
    explicit UIImageWidget(std::string name = {});

    void SetImageLoader(UIImageLoader loader);
    bool SetSource(const std::string& source);
    const std::string& GetSource() const { return m_source; }
    bool IsLoaded() const { return m_image.textureHandle != nullptr; }
    void SetFit(ImageFit fit) { m_fit = fit; MarkDirty(); }
    ImageFit GetFit() const { return m_fit; }
    void SetTint(uint32_t color) { m_tint = color; MarkDirty(); }

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    UIImageLoader m_loader;
    std::string m_source;
    UIImage m_image;
    ImageFit m_fit = ImageFit::Contain;
    uint32_t m_tint = 0xFFFFFFFFu;
};

} // namespace LambUI