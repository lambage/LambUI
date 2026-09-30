#pragma once

#include "lambui_export.h"
#include "UITypes.h"
#include "UIWidget.h"

namespace LambUI {

// Punches a "hole" for custom rendering (e.g. an embedded 3D character
// viewport) at this widget's resolved screen rect, in the middle of the 2D UI pass.
class LAMBUI_API UICanvasWidget : public UIWidget {
public:
    explicit UICanvasWidget(std::string name = {});

    void SetRenderCallback(UICustomRenderCallback callback, void* userData = nullptr);

protected:
    void OnGenerateRenderCommands(std::vector<UIRenderCommand>& bucket) override;

private:
    UICustomRenderCallback m_renderCallback;
    void* m_userData = nullptr;
};

} // namespace LambUI
