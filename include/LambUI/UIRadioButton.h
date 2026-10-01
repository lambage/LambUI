#pragma once

#include "UICheckBox.h"

namespace LambUI {

class LAMBUI_API UIRadioButton : public UICheckBox {
public:
    explicit UIRadioButton(std::string name = {});
    ~UIRadioButton() override;
    void SetGroup(std::string group);
    const std::string& GetGroup() const { return m_group; }
    void SetChecked(bool checked) override;

protected:
    void Activate() override;
    void GenerateIndicator(std::vector<UIRenderCommand>& bucket, float x, float y, uint32_t color) override;

private:
    std::string m_group;
};
} // namespace LambUI