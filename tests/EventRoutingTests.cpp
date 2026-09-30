#include "LambUI/UIButton.h"
#include "LambUI/UIManager.h"
#include <gtest/gtest.h>
#include <memory>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    void SubmitRenderCommands(const std::vector<UIRenderCommand>&) override {}
};
} // namespace

TEST(EventRouting, ClickOnWidgetFiresOnClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 40.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool clicked = false;
    button->RegisterCallback(UIEventType::OnClick, [&clicked](const UIEventData&) { clicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_TRUE(clicked);
}

TEST(EventRouting, ReleasingOutsideOriginalWidgetDoesNotFireClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 40.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool clicked = false;
    button->RegisterCallback(UIEventType::OnClick, [&clicked](const UIEventData&) { clicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(500.0f, 500.0f);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_FALSE(clicked);
}

TEST(EventRouting, TopMostOverlappingWidgetCapturesHitFirst) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(800.0f, 600.0f);

    UIButton* back = manager.GetRoot().CreateChild<UIButton>("Back");
    back->SetSize(100.0f, 100.0f);
    back->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    UIButton* front = manager.GetRoot().CreateChild<UIButton>("Front");
    front->SetSize(50.0f, 50.0f);
    front->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 0.0f, 0.0f);

    bool backClicked = false;
    bool frontClicked = false;
    back->RegisterCallback(UIEventType::OnClick, [&backClicked](const UIEventData&) { backClicked = true; });
    front->RegisterCallback(UIEventType::OnClick, [&frontClicked](const UIEventData&) { frontClicked = true; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_TRUE(frontClicked);
    EXPECT_FALSE(backClicked);
}
