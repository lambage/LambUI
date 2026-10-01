#include "LambUI/UIButton.h"
#include "LambUI/UIDropDownBox.h"
#include "LambUI/UIManager.h"
#include "LambUI/UISlider.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

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

TEST(EventRouting, InjectedClickBubblesFromLeafToRootWithOriginalPayload) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetSize(100.0f, 100.0f);
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    leaf->SetAllPoints(parent);

    std::vector<std::string> visited;
    for (UIWidget* widget : {leaf, parent, &manager.GetRoot()}) {
        widget->RegisterCallback(UIEventType::OnClick, [&, widget](const UIEventData& data) {
            visited.push_back(widget->GetName());
            EXPECT_EQ(data.type, UIEventType::OnClick);
            EXPECT_FLOAT_EQ(data.mouseX, 10.0f);
            EXPECT_FLOAT_EQ(data.mouseY, 20.0f);
            EXPECT_EQ(data.button, MouseButton::Right);
            EXPECT_FALSE(data.isDown);
        });
    }

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 20.0f);
    manager.InjectMouseButton(MouseButton::Right, true);
    manager.InjectMouseButton(MouseButton::Right, false);

    EXPECT_EQ(visited, (std::vector<std::string>{"Leaf", "Parent", "Root"}));
}

TEST(EventRouting, HandledEventStopsAtParent) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    std::vector<std::string> visited;
    parent->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        visited.push_back("Parent");
        data.handled = true;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) {
        visited.push_back("Root");
    });

    UIEventData data{UIEventType::OnClick};
    leaf->FireEvent(data);

    EXPECT_TRUE(data.handled);
    EXPECT_EQ(visited, (std::vector<std::string>{"Parent"}));
}

TEST(EventRouting, MouseEnterAndLeaveRemainTargetLocal) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* leaf = manager.GetRoot().CreateChild<UIWidget>("Leaf");
    int leafEvents = 0;
    int parentEvents = 0;
    for (UIEventType type : {UIEventType::OnMouseEnter, UIEventType::OnMouseLeave}) {
        leaf->RegisterCallback(type, [&](const UIEventData&) { ++leafEvents; });
        manager.GetRoot().RegisterCallback(type, [&](const UIEventData&) { ++parentEvents; });
        leaf->FireEvent(UIEventData{type});
    }

    EXPECT_EQ(leafEvents, 2);
    EXPECT_EQ(parentEvents, 0);
}

TEST(EventRouting, HandledLeafClickDoesNotReachAncestors) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* leaf = manager.GetRoot().CreateChild<UIWidget>("Leaf");
    int parentClicks = 0;
    leaf->RegisterCallback(UIEventType::OnClick, [](const UIEventData& data) {
        data.handled = true;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) {
        ++parentClicks;
    });

    leaf->FireEvent(UIEventData{UIEventType::OnClick});

    EXPECT_EQ(parentClicks, 0);
}

TEST(EventRouting, CapturedReleaseBubblesButReleaseOutsideDoesNotClick) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIWidget* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetSize(100.0f, 100.0f);
    parent->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    UIWidget* leaf = parent->CreateChild<UIWidget>("Leaf");
    leaf->SetAllPoints(parent);
    std::vector<UIEventType> events;
    for (UIEventType type : {UIEventType::OnMouseDown, UIEventType::OnMouseUp, UIEventType::OnClick}) {
        parent->RegisterCallback(type, [&](const UIEventData& data) {
            events.push_back(data.type);
            EXPECT_EQ(data.isDown, data.type == UIEventType::OnMouseDown);
            if (data.type == UIEventType::OnMouseUp) {
                EXPECT_FLOAT_EQ(data.mouseX, 500.0f);
            }
        });
    }

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(500.0f, 500.0f);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_EQ(events, (std::vector<UIEventType>{UIEventType::OnMouseDown, UIEventType::OnMouseUp}));
}

TEST(EventRouting, ControlConsumesPressAndReleaseButStillRunsItsCallbacks) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIButton* parent = manager.GetRoot().CreateChild<UIButton>("Parent");
    UIButton* button = parent->CreateChild<UIButton>("Button");
    UIWidget* label = button->CreateChild<UIWidget>("Label");
    int callbacks = 0;
    for (UIEventType type : {UIEventType::OnMouseDown, UIEventType::OnMouseUp}) {
        button->RegisterCallback(type, [&](const UIEventData& data) {
            EXPECT_TRUE(data.handled);
            ++callbacks;
        });
        label->FireEvent(UIEventData{type});
        EXPECT_EQ(parent->GetState(), ControlState::Normal);
    }

    EXPECT_EQ(button->GetState(), ControlState::Hovered);
    EXPECT_EQ(callbacks, 2);
}

TEST(EventRouting, DropdownOptionSelectionConsumesClickWithoutReopening) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIDropDownBox* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("Dropdown");
    dropdown->SetSize(100.0f, 30.0f);
    dropdown->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    dropdown->SetOptions({"First", "Second"});
    int dropdownClicks = 0;
    int rootClicks = 0;
    int valueChanges = 0;
    dropdown->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        EXPECT_TRUE(data.handled);
        ++dropdownClicks;
    });
    manager.GetRoot().RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++rootClicks; });
    manager.GetRoot().RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++valueChanges; });

    manager.Update(0.0f);
    manager.InjectMouseMove(10.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    ASSERT_TRUE(dropdown->IsExpanded());
    manager.InjectMouseMove(10.0f, 60.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);

    EXPECT_EQ(dropdown->GetSelectedIndex(), 1);
    EXPECT_FALSE(dropdown->IsExpanded());
    EXPECT_EQ(dropdownClicks, 1);
    EXPECT_EQ(rootClicks, 0);
    EXPECT_EQ(valueChanges, 1);
}

TEST(EventRouting, WidgetGeneratedEventsBubbleIndependently) {
    UIManager manager(std::make_shared<NullRenderer>());
    UISlider* slider = manager.GetRoot().CreateChild<UISlider>("Slider");
    int valueChanges = 0;
    manager.GetRoot().RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData& data) {
        ++valueChanges;
        data.handled = true;
    });

    slider->SetValue(0.25f);
    slider->SetValue(0.75f);

    EXPECT_EQ(valueChanges, 2);
}
