#include "LambUI/UIButton.h"
#include "LambUI/UICheckBox.h"
#include "LambUI/UIDropDownBox.h"
#include "LambUI/UIInputBox.h"
#include "LambUI/UIManager.h"
#include "LambUI/UISlider.h"
#include "LambUI/UITabControl.h"
#include "LambUI/UITreeView.h"
#include "LambUI/UIMenuBar.h"
#include "LambUI/UIRadioButton.h"
#include "LambUI/UITextureWidget.h"
#include "LambUI/UITextWidget.h"
#include "LambUI/UIWindow.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace LambUI;

namespace {
class NullRenderer : public IRenderer {
public:
    std::vector<UIRenderCommand> commands;
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& bucket) override { commands = bucket; }
};
} // namespace

TEST(PointerShape, WindowEdgesCornersAndPolicyMatchResizeHitRegions) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* window = manager.GetRoot().CreateChild<UIWindow>("Window");
    window->SetBounds(20, 20, 240, 160);
    manager.Update(0);
    struct Sample { float x; float y; PointerShape shape; };
    const Sample samples[] = {
        {21, 80, PointerShape::ResizeEW}, {259, 80, PointerShape::ResizeEW},
        {100, 21, PointerShape::ResizeNS}, {100, 179, PointerShape::ResizeNS},
        {21, 21, PointerShape::ResizeNWSE}, {259, 179, PointerShape::ResizeNWSE},
        {259, 21, PointerShape::ResizeNESW}, {21, 179, PointerShape::ResizeNESW},
        {100, 40, PointerShape::Arrow}, {100, 100, PointerShape::Arrow},
        {19, 80, PointerShape::Arrow}, {260, 80, PointerShape::Arrow}
    };
    for (const auto& sample : samples) {
        manager.InjectMouseMove(sample.x, sample.y);
        EXPECT_EQ(manager.GetPointerShape(), sample.shape);
    }
    const auto close = window->GetButtonRect(WindowButton::Close);
    manager.InjectMouseMove(close.x + 1, close.y + 1);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    manager.InjectMouseMove(21, 80);
    window->SetResizable(false);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    window->SetResizable(true);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::ResizeEW);
    window->SetVisible(false);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    window->SetVisible(true);
    window->Minimize();
    manager.Update(0);
    manager.InjectMouseMove(21, 21);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    window->Maximize();
    manager.Update(0);
    manager.InjectMouseMove(1, 1);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
}

TEST(PointerShape, CaptureRetainsResizeDirectionAndReleaseRestoresHitShape) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* window = manager.GetRoot().CreateChild<UIWindow>("Window");
    window->SetBounds(20, 20, 240, 160);
    manager.Update(0);
    manager.InjectMouseMove(259, 179);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(800, 800);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::ResizeNWSE);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    window->SetBounds(20, 20, 240, 160);
    manager.Update(0);
    manager.InjectMouseMove(21, 80);
    manager.InjectMouseButton(MouseButton::Left, true);
    window->SetResizable(false);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    manager.InjectMouseButton(MouseButton::Left, false);
    window->SetResizable(true);
    manager.InjectMouseButton(MouseButton::Right, true);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    manager.InjectMouseButton(MouseButton::Right, false);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::ResizeEW);
}

TEST(HoverCallbacks, ApplicationHandlersObserveTransitionsAndNativeLeave) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 20, 20);
    button->SetSize(100, 40);
    auto* label = button->CreateChild<UITextWidget>("Label");
    label->SetAllPoints(button);
    label->SetMouseEnabled(false);
    int enters = 0, leaves = 0, parentEnters = 0;
    button->RegisterCallback(UIEventType::OnMouseEnter, [&](const UIEventData& data) {
        ++enters;
        EXPECT_EQ(button->GetState(), ControlState::Hovered);
        EXPECT_FLOAT_EQ(data.mouseX, 30);
    });
    button->RegisterCallback(UIEventType::OnMouseLeave, [&](const UIEventData&) { ++leaves; });
    manager.GetRoot().RegisterCallback(UIEventType::OnMouseEnter, [&](const UIEventData&) { ++parentEnters; });
    manager.InjectMouseLeave();
    manager.Update(0);
    manager.InjectMouseMove(30, 30);
    manager.InjectMouseMove(30, 35);
    manager.Update(0);
    EXPECT_EQ(enters, 1);
    EXPECT_EQ(parentEnters, 0);
    manager.InjectMouseLeave();
    manager.InjectMouseLeave();
    manager.Update(0);
    EXPECT_EQ(leaves, 1);
    EXPECT_EQ(button->GetState(), ControlState::Normal);
    EXPECT_EQ(manager.GetPointerShape(), PointerShape::Arrow);
    manager.InjectMouseMove(30, 30);
    EXPECT_EQ(enters, 2);
    button->SetVisible(false);
    manager.Update(0);
    EXPECT_EQ(leaves, 2);
    button->RegisterCallback(UIEventType::OnMouseEnter, {});
    button->SetVisible(true);
    manager.Update(0);
    EXPECT_EQ(enters, 2);
    EXPECT_EQ(button->GetState(), ControlState::Hovered);
}

TEST(HoverCallbacks, NativeLeaveDuringCaptureIsDeliveredOnRelease) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 20, 20);
    button->SetSize(100, 40);
    int leaves = 0, clicks = 0;
    button->RegisterCallback(UIEventType::OnMouseLeave, [&](const UIEventData&) { ++leaves; });
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0);
    manager.InjectMouseMove(30, 30);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseLeave();
    manager.Update(0);
    EXPECT_EQ(leaves, 0);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(leaves, 1);
    EXPECT_EQ(clicks, 0);
}

TEST(Clipboard, InputShortcutsNormalizePasteAndRespectReadOnly) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    std::string clipboard;
    bool writable = true;
    manager.SetClipboardCallbacks([&](std::string& text) { text = clipboard; return true; },
        [&](const std::string& text) { if (!writable) return false; clipboard = text; return true; });
    manager.InjectKeyEvent(ScanCode::Tab, true);
    input->SetText(u8"one \u00e9 two");
    input->SetSelection(4, 6);
    int changes = 0;
    input->RegisterCallback(UIEventType::OnTextChanged, [&](const UIEventData&) { ++changes; });
    manager.InjectKeyEvent(ScanCode::LeftControl, true);
    manager.InjectKeyEvent(ScanCode::C, true);
    EXPECT_EQ(clipboard, u8"\u00e9");
    EXPECT_EQ(changes, 0);
    writable = false;
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_EQ(input->GetSelectedText(), clipboard);
    writable = true;
    manager.InjectKeyEvent(ScanCode::X, true);
    manager.InjectKeyEvent(ScanCode::X, false);
    EXPECT_EQ(input->GetText(), "one  two");
    EXPECT_EQ(changes, 1);
    clipboard = u8"\u03bb\r\nnext\tline\n";
    manager.InjectKeyEvent(ScanCode::V, true);
    EXPECT_EQ(input->GetText(), u8"one \u03bb next line  two");
    EXPECT_EQ(changes, 2);
    input->SelectAll();
    input->SetEditingEnabled(false);
    EXPECT_TRUE(manager.InjectCopy());
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(changes, 2);
    input->SetEditingEnabled(true);
    input->SetMultiline(true);
    input->SelectAll();
    clipboard = "first\r\nsecond\rthird";
    EXPECT_TRUE(manager.InjectPaste());
    EXPECT_EQ(input->GetText(), "first\nsecond\nthird");
    EXPECT_EQ(changes, 3);
    input->SelectAll();
    clipboard.clear();
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(input->GetSelectedText(), input->GetText());
    clipboard = std::string("\0\x01\x7f", 3);
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(input->GetSelectedText(), input->GetText());
    EXPECT_EQ(changes, 3);
    manager.SetClipboardCallbacks({}, {});
    EXPECT_FALSE(manager.InjectCopy());
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_FALSE(manager.InjectPaste());
}

TEST(Clipboard, LabelsOptInToMouseKeyboardSelectionAndCopyOnly) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* label = manager.GetRoot().CreateChild<UITextWidget>("Label");
    label->SetSize(80, 64);
    label->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10, 10);
    label->SetText(u8"ab\u00e9d\nnext");
    label->SetWordWrap(true);
    label->SetFocusRingEnabled(false);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    label->SelectAll();
    EXPECT_TRUE(label->GetSelectedText().empty());
    label->SetSelectionEnabled(true);
    manager.Update(0);
    manager.InjectMouseMove(18, 15);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(42, 15);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(manager.GetFocusedWidget(), label);
    EXPECT_EQ(label->GetSelectedText(), u8"b\u00e9d");
    manager.InjectKeyEvent(ScanCode::RightShift, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(label->GetSelectedText(), u8"b\u00e9d\nnext");
    manager.InjectKeyEvent(ScanCode::RightShift, false);
    std::string clipboard;
    manager.SetClipboardCallbacks([](std::string& text) { text = "replacement"; return true; },
        [&](const std::string& text) { clipboard = text; return true; });
    manager.InjectKeyEvent(ScanCode::RightControl, true);
    manager.InjectKeyEvent(ScanCode::A, true);
    manager.InjectKeyEvent(ScanCode::C, true);
    EXPECT_EQ(clipboard, label->GetText());
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_FALSE(manager.InjectPaste());
    manager.InjectCharacter(U'!');
    EXPECT_EQ(label->GetText(), clipboard);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0x287EA8FFu; }), 2);
    label->SetSelection(4, 2);
    EXPECT_EQ(label->GetSelectedText(), u8"\u00e9");
    label->SetSelectionEnabled(false);
    EXPECT_TRUE(label->GetSelectedText().empty());
    EXPECT_FALSE(manager.InjectCopy());
    manager.Update(0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
    label->SetSelectionEnabled(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::A, true);
    EXPECT_EQ(label->GetSelectedText(), label->GetText());
    label->SetText("short");
    EXPECT_TRUE(label->GetSelectedText().empty());
}

TEST(Clipboard, FailedReadsAndIneligibleFocusLeaveTextUntouched) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    auto* input = parent->CreateChild<UIInputBox>("Input");
    input->SetText("original");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    input->SelectAll();
    int reads = 0;
    int writes = 0;
    bool readable = false;
    manager.SetClipboardCallbacks([&](std::string& text) {
        ++reads;
        text = "paste";
        return readable;
    }, [&](const std::string&) { ++writes; return true; });
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(input->GetSelectedText(), "original");
    readable = true;
    parent->SetVisible(false);
    EXPECT_FALSE(manager.InjectCopy());
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(reads, 1);
    EXPECT_EQ(writes, 0);
    parent->SetVisible(true);
    input->SetKeyboardEnabled(false);
    EXPECT_FALSE(manager.InjectPaste());
    input->SetKeyboardEnabled(true);
    input->SetSelectionEnabled(false);
    EXPECT_FALSE(manager.InjectCopy());
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_EQ(writes, 0);
    EXPECT_TRUE(manager.InjectPaste());
    EXPECT_EQ(input->GetText(), "originalpaste");
    auto* popup = manager.GetOverlayRoot().CreateChild<UIWidget>("Popup");
    popup->SetSize(120, 60);
    auto* popupInput = popup->CreateChild<UIInputBox>("PopupInput");
    popupInput->SetText("popup");
    popupInput->SetKeyboardEnabled(false);
    manager.ShowPopup(*popup, 0, 0);
    EXPECT_FALSE(manager.InjectPaste());
    popupInput->SetKeyboardEnabled(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), popupInput);
    popupInput->SelectAll();
    EXPECT_TRUE(manager.InjectPaste());
    EXPECT_EQ(popupInput->GetText(), "paste");
    EXPECT_EQ(input->GetText(), "originalpaste");
    manager.ClosePopup();
    EXPECT_EQ(manager.GetFocusedWidget(), input);
}

TEST(Clipboard, CallbackFocusAndSelectionChangesCancelPendingEdits) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIInputBox>("First");
    auto* second = manager.GetRoot().CreateChild<UIInputBox>("Second");
    first->SetText("first");
    second->SetText("second");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    first->SelectAll();
    manager.SetClipboardCallbacks([&](std::string& text) {
        text = "replacement";
        manager.InjectKeyEvent(ScanCode::Tab, true);
        return true;
    }, {});
    EXPECT_FALSE(manager.InjectPaste());
    EXPECT_EQ(manager.GetFocusedWidget(), second);
    EXPECT_EQ(first->GetText(), "first");
    EXPECT_EQ(second->GetText(), "second");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.SetClipboardCallbacks({}, [&](const std::string&) {
        first->SetSelection(1, 3);
        manager.SetClipboardCallbacks({}, {});
        return true;
    });
    EXPECT_FALSE(manager.InjectCut());
    EXPECT_EQ(first->GetText(), "first");
    EXPECT_EQ(first->GetSelectedText(), "ir");
}

TEST(Clipboard, SelectableLabelConsumesClicksAndCancelsDragOnFocusLoss) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* button = manager.GetRoot().CreateChild<UIButton>("ParentButton");
    button->SetSize(120, 32);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    auto* label = button->CreateChild<UITextWidget>("Label");
    label->SetAllPoints(button);
    label->SetText("abcdef");
    label->SetSelectionEnabled(true);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0);
    manager.InjectMouseMove(8, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(24, 8);
    EXPECT_EQ(label->GetSelectedText(), "bc");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), input);
    manager.InjectMouseMove(40, 8);
    EXPECT_EQ(label->GetSelectedText(), "bc");
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(clicks, 0);
    label->SetSelectionEnabled(false);
    manager.InjectMouseMove(8, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    EXPECT_EQ(clicks, 1);
}

TEST(DialogDefault, SingleLineInputRoutesPairedEnterWithoutMovingFocus) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* dialog = manager.GetRoot().CreateChild<UIWidget>("Dialog");
    auto* input = dialog->CreateChild<UIInputBox>("Input");
    auto* accept = dialog->CreateChild<UIButton>("Accept");
    accept->SetSize(100, 30);
    accept->SetPoint(AnchorPoint::TopLeft, dialog, AnchorPoint::TopLeft);
    accept->SetPressedColor(0x123456FFu);
    int clicks = 0;
    int submissions = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    ASSERT_TRUE(manager.SetDefaultButton(*dialog, accept));
    EXPECT_EQ(manager.GetDefaultButton(*dialog), accept);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0x123456FFu; }), 1);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    EXPECT_TRUE(input->IsFocused());
    EXPECT_FALSE(accept->HasKeyboardFocus());
    EXPECT_EQ(clicks, 0);
    EXPECT_EQ(submissions, 0);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    EXPECT_FALSE(accept->IsKeyboardPressed());
}

TEST(DialogDefault, NearestScopeWinsAndClearingBlocksOuterDefaults) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* outer = manager.GetRoot().CreateChild<UIWidget>("Outer");
    auto* inner = outer->CreateChild<UIWidget>("Inner");
    auto* input = inner->CreateChild<UIInputBox>("Input");
    auto* innerButton = inner->CreateChild<UIButton>("InnerAccept");
    auto* outerButton = outer->CreateChild<UIButton>("OuterAccept");
    int innerClicks = 0;
    int outerClicks = 0;
    int submissions = 0;
    innerButton->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++innerClicks; });
    outerButton->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++outerClicks; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    const auto enter = [&] {
        manager.InjectKeyEvent(ScanCode::Enter, true);
        manager.InjectKeyEvent(ScanCode::Enter, false);
    };
    ASSERT_TRUE(manager.SetDefaultButton(*outer, outerButton));
    enter();
    EXPECT_EQ(outerClicks, 0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), input);
    enter();
    EXPECT_EQ(outerClicks, 1);
    ASSERT_TRUE(manager.SetDefaultButton(*inner, innerButton));
    enter();
    EXPECT_EQ(innerClicks, 1);
    innerButton->SetKeyboardEnabled(false);
    enter();
    EXPECT_EQ(submissions, 1);
    EXPECT_EQ(outerClicks, 1);
    ASSERT_TRUE(manager.SetDefaultButton(*inner, nullptr));
    EXPECT_EQ(manager.GetDefaultButton(*inner), nullptr);
    enter();
    EXPECT_EQ(submissions, 2);
    EXPECT_EQ(innerClicks, 1);
    EXPECT_EQ(outerClicks, 1);
}

TEST(DialogDefault, RegistrationRejectsForeignWidgetsAndWindowsIsolateDefaults) {
    UIManager manager(std::make_shared<NullRenderer>());
    UIManager foreign(std::make_shared<NullRenderer>());
    auto* window = manager.GetRoot().CreateChild<UIWindow>("Window");
    auto* input = window->GetContent()->CreateChild<UIInputBox>("Input");
    auto* accept = window->GetContent()->CreateChild<UIButton>("Accept");
    auto* sibling = manager.GetRoot().CreateChild<UIWindow>("Sibling");
    auto* siblingInput = sibling->GetContent()->CreateChild<UIInputBox>("SiblingInput");
    auto* rootButton = manager.GetRoot().CreateChild<UIButton>("RootAccept");
    auto* foreignButton = foreign.GetRoot().CreateChild<UIButton>("Foreign");
    int clicks = 0;
    int rootClicks = 0;
    int submissions = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    rootButton->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++rootClicks; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    siblingInput->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    const auto enter = [&] {
        manager.InjectKeyEvent(ScanCode::Enter, true);
        manager.InjectKeyEvent(ScanCode::Enter, false);
    };
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), rootButton));
    EXPECT_FALSE(manager.SetDefaultButton(manager.GetRoot(), accept));
    EXPECT_FALSE(manager.SetDefaultButton(*window, foreignButton));
    EXPECT_FALSE(manager.SetDefaultButton(foreign.GetRoot(), foreignButton));
    EXPECT_FALSE(manager.SetDefaultButton(*window, rootButton));
    EXPECT_FALSE(manager.SetDefaultButton(*accept, accept));
    EXPECT_FALSE(manager.SetDefaultButton(manager.GetOverlayRoot(), nullptr));
    EXPECT_EQ(manager.GetDefaultButton(manager.GetRoot()), rootButton);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), input);
    enter();
    EXPECT_EQ(submissions, 1);
    EXPECT_EQ(rootClicks, 0);
    ASSERT_TRUE(manager.SetDefaultButton(*window, accept));
    enter();
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), siblingInput);
    enter();
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(rootClicks, 0);
    EXPECT_EQ(submissions, 2);
}

TEST(DialogDefault, UnavailableButtonsFallBackToInputSubmission) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* buttons = manager.GetRoot().CreateChild<UIWidget>("Buttons");
    auto* accept = buttons->CreateChild<UIButton>("Accept");
    int clicks = 0;
    int submissions = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(submissions, 1);
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), accept));
    for (int unavailable = 0; unavailable < 4; ++unavailable) {
        accept->SetVisible(unavailable != 0);
        buttons->SetVisible(unavailable != 1);
        accept->SetMouseEnabled(unavailable != 2);
        accept->SetKeyboardEnabled(unavailable != 3);
        manager.InjectKeyEvent(ScanCode::Enter, true);
        manager.InjectKeyEvent(ScanCode::Enter, false);
        EXPECT_EQ(submissions, unavailable + 2);
        EXPECT_EQ(clicks, 0);
    }
    accept->SetKeyboardEnabled(true);
    input->SetEditingEnabled(false);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(submissions, 5);
}

TEST(DialogDefault, MultilineControlsAndModifiedEnterKeepTheirExistingBehavior) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* checkbox = manager.GetRoot().CreateChild<UICheckBox>("Checkbox");
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("Dropdown");
    auto* other = manager.GetRoot().CreateChild<UIButton>("Other");
    auto* accept = manager.GetRoot().CreateChild<UIButton>("Accept");
    dropdown->SetOptions({"First", "Second"});
    int clicks = 0;
    int otherClicks = 0;
    int submissions = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    other->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++otherClicks; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), accept));
    const auto enter = [&] {
        manager.InjectKeyEvent(ScanCode::Enter, true);
        manager.InjectKeyEvent(ScanCode::Enter, false);
    };
    manager.InjectKeyEvent(ScanCode::Tab, true);
    input->SetMultiline(true);
    enter();
    EXPECT_EQ(input->GetText(), "\n");
    input->SetEditingEnabled(false);
    enter();
    EXPECT_EQ(input->GetText(), "\n");
    input->SetMultiline(false);
    for (auto modifier : {ScanCode::LeftShift, ScanCode::RightShift, ScanCode::LeftControl, ScanCode::RightControl}) {
        manager.InjectKeyEvent(modifier, true);
        enter();
        manager.InjectKeyEvent(modifier, false);
    }
    EXPECT_EQ(submissions, 4);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), checkbox);
    enter();
    EXPECT_TRUE(checkbox->IsChecked());
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), dropdown);
    enter();
    EXPECT_TRUE(dropdown->IsExpanded());
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), other);
    enter();
    EXPECT_EQ(otherClicks, 1);
    EXPECT_EQ(clicks, 0);
}

TEST(DialogDefault, FocusChangesAndReconfigurationCancelUntilRelease) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* next = manager.GetRoot().CreateChild<UIInputBox>("Next");
    auto* accept = manager.GetRoot().CreateChild<UIButton>("Accept");
    auto* replacement = manager.GetRoot().CreateChild<UIButton>("Replacement");
    int clicks = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    replacement->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), accept));
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), input);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    ASSERT_EQ(manager.GetFocusedWidget(), next);
    EXPECT_FALSE(accept->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), replacement));
    EXPECT_FALSE(accept->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), replacement));
    EXPECT_TRUE(replacement->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), nullptr));
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
}

TEST(DialogDefault, EligibilityChangesCancelOnRenderUpdateAndKeyInjection) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* accept = manager.GetRoot().CreateChild<UIButton>("Accept");
    int clicks = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), accept));
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    accept->SetVisible(false);
    manager.Render();
    EXPECT_FALSE(accept->IsKeyboardPressed());
    accept->SetVisible(true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    accept->SetMouseEnabled(false);
    manager.Update(0);
    accept->SetMouseEnabled(true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    input->SetMultiline(true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    EXPECT_TRUE(input->GetText().empty());
    input->SetMultiline(false);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::LeftControl, true);
    manager.InjectKeyEvent(ScanCode::LeftControl, false);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    input->SetVisible(false);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
}

TEST(DialogDefault, PopupIsolationAndCallbacksDoNotLeakOrRepeatActivation) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* accept = manager.GetRoot().CreateChild<UIButton>("Accept");
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Menu");
    int clicks = 0;
    int menuClicks = 0;
    accept->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) {
        ++clicks;
        EXPECT_FALSE(accept->IsKeyboardPressed());
        EXPECT_TRUE(manager.SetDefaultButton(manager.GetRoot(), nullptr));
        menu->Open(0, 0, input);
    });
    menu->SetItems({{"Action", [&] { ++menuClicks; }}});
    ASSERT_TRUE(manager.SetDefaultButton(manager.GetRoot(), accept));
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    menu->Open(0, 0, input);
    EXPECT_FALSE(accept->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    EXPECT_EQ(menuClicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(menuClicks, 1);
    EXPECT_EQ(manager.GetActivePopup(), nullptr);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(manager.GetActivePopup(), menu);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(menuClicks, 1);
    EXPECT_EQ(clicks, 1);
}

TEST(FocusVisualization, RingFollowsFocusAndHonorsAppearanceAndEligibility) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    for (auto* widget : std::vector<UIWidget*>{button, input}) {
        widget->SetSize(100, 30);
        widget->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft,
                         widget == button ? 10.0f : 120.0f, 10);
        widget->SetFocusRingColor(0xFFCC00FFu);
    }
    const auto ringCount = [&] {
        manager.Render();
        return std::count_if(renderer->commands.begin(), renderer->commands.end(),
            [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; });
    };
    manager.Update(0);
    EXPECT_EQ(ringCount(), 0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(ringCount(), 4);
    const auto lastRing = std::find_if(renderer->commands.rbegin(), renderer->commands.rend(),
        [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; });
    ASSERT_NE(lastRing, renderer->commands.rend());
    EXPECT_FLOAT_EQ(lastRing->x, 108);
    button->SetFocusRingEnabled(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    button->SetFocusRingEnabled(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(ringCount(), 4);
    EXPECT_FLOAT_EQ(renderer->commands.back().x, 218);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    EXPECT_EQ(ringCount(), 4);
    button->SetKeyboardEnabled(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
    manager.InjectMouseMove(125, 15);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    EXPECT_EQ(ringCount(), 4);
    input->SetVisible(false);
    EXPECT_EQ(ringCount(), 0);
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
}

TEST(FocusVisualization, RingRespectsChildPaintingSiblingOrderAndAncestorClip) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100, 40);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(100, 100);
    scroll->SetScrollbarsEnabled(false);
    auto* button = scroll->GetContent()->CreateChild<UIButton>("Button");
    button->SetSize(80, 30);
    button->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0, 20);
    button->SetFocusRingColor(0xFFCC00FFu);
    auto* child = button->CreateChild<UITextureWidget>("Child");
    child->SetAllPoints(button);
    child->SetTint(0x123456FFu);
    auto* later = scroll->GetContent()->CreateChild<UITextureWidget>("LaterSibling");
    later->SetAllPoints(button);
    later->SetTint(0x654321FFu);
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    scroll->SetScrollOffset(0, 0);
    manager.Update(0);
    manager.Render();
    int clipDepth = 0;
    int ringCount = 0;
    bool childPainted = false;
    bool siblingPainted = false;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::PushScissor) ++clipDepth;
        if (command.type == RenderCommandType::PopScissor) --clipDepth;
        if (command.color == 0x123456FFu) childPainted = true;
        if (command.color == 0x654321FFu) siblingPainted = true;
        if (command.color != 0xFFCC00FFu) continue;
        ++ringCount;
        EXPECT_GT(clipDepth, 0);
        EXPECT_TRUE(childPainted);
        EXPECT_FALSE(siblingPainted);
    }
    EXPECT_EQ(ringCount, 4);
    EXPECT_EQ(clipDepth, 0);
    EXPECT_TRUE(siblingPainted);
}

TEST(FocusVisualization, TreePopupRestorationAndTinyBoundsUseSharedDecoration) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetSize(100, 60);
    tree->SetPadding({5, 5, 5, 5});
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tree->SetFocusRingColor(0xFFCC00FFu);
    tree->AddNode(0, "Item");
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Menu");
    menu->SetItems({{"Item", {}}});
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Render();
    EXPECT_EQ(manager.GetFocusedWidget(), tree);
    EXPECT_EQ(renderer->commands.back().color, 0xFFCC00FFu);
    int clipDepth = 0;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::PushScissor) ++clipDepth;
        if (command.type == RenderCommandType::PopScissor) --clipDepth;
        if (command.color == 0xFFCC00FFu) EXPECT_EQ(clipDepth, 0);
    }
    menu->Open(110, 10, tree);
    manager.GetFocusedWidget()->SetFocusRingColor(0xCC00FFFFu);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0xFFCC00FFu; }), 0);
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0xCC00FFFFu; }), 4);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    manager.Render();
    EXPECT_EQ(manager.GetFocusedWidget(), tree);
    EXPECT_EQ(renderer->commands.back().color, 0xFFCC00FFu);
    tree->SetSize(0.5f, 0.5f);
    tree->SetPadding({});
    manager.Update(0);
    manager.Render();
    int tinyStrokes = 0;
    for (const auto& command : renderer->commands) {
        if (command.color != 0x172127FFu) continue;
        ++tinyStrokes;
        EXPECT_GT(command.width, 0);
        EXPECT_GT(command.height, 0);
        EXPECT_GE(command.x, 0);
        EXPECT_GE(command.y, 0);
        EXPECT_LE(command.x + command.width, 0.5f);
        EXPECT_LE(command.y + command.height, 0.5f);
    }
    EXPECT_EQ(tinyStrokes, 2);
    tree->SetSize(0, 0);
    manager.Update(0);
    manager.Render();
    EXPECT_EQ(std::count_if(renderer->commands.begin(), renderer->commands.end(),
        [](const UIRenderCommand& command) { return command.color == 0x172127FFu; }), 0);
}

TEST(FocusVisualization, ButtonShowsPairedKeyboardPressAndCancelsOnFocusLoss) {
    auto renderer = std::make_shared<NullRenderer>();
    UIManager manager(renderer);
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    button->SetSize(100.0f, 30.0f);
    button->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    button->SetNormalColor(0x202020FFu);
    button->SetPressedColor(0x909090FFu);
    manager.GetRoot().CreateChild<UIButton>("Next");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.Render();
    ASSERT_FALSE(renderer->commands.empty());
    EXPECT_EQ(renderer->commands.front().color, 0x909090FFu);
    EXPECT_EQ(button->GetState(), ControlState::Normal);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_TRUE(button->IsKeyboardPressed());
    manager.InjectKeyEvent(ScanCode::Enter, false);
    manager.Render();
    EXPECT_EQ(renderer->commands.front().color, 0x202020FFu);
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    manager.Render();
    EXPECT_FALSE(button->IsKeyboardPressed());
    EXPECT_EQ(renderer->commands.front().color, 0x202020FFu);
    EXPECT_EQ(clicks, 1);
}

TEST(KeyboardNavigation, TabFocusesInputsInTreeOrderAndWraps) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIInputBox>("First");
    auto* second = manager.GetRoot().CreateChild<UIInputBox>("Second");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'a');
    manager.InjectKeyEvent(ScanCode::Tab, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'b');
    manager.InjectKeyEvent(ScanCode::Tab, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'c');
    EXPECT_EQ(first->GetText(), "ac");
    EXPECT_EQ(second->GetText(), "b");
}

TEST(KeyboardNavigation, ShiftTabSkipsHiddenAndDisabledControls) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIButton>("First");
    auto* disabled = manager.GetRoot().CreateChild<UICheckBox>("Disabled");
    disabled->SetEnabled(false);
    auto* hidden = manager.GetRoot().CreateChild<UIWidget>("Hidden");
    hidden->CreateChild<UIInputBox>("HiddenInput");
    hidden->SetVisible(false);
    auto* last = manager.GetRoot().CreateChild<UIInputBox>("Last");
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::RightShift, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    manager.InjectKeyEvent(ScanCode::Tab, false);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), first);
    manager.InjectKeyEvent(ScanCode::RightShift, false);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    last->SetVisible(false);
    manager.InjectCharacter(U'x');
    EXPECT_EQ(manager.GetFocusedWidget(), nullptr);
    EXPECT_FALSE(last->IsFocused());
    EXPECT_TRUE(last->GetText().empty());
}

TEST(KeyboardNavigation, ActivationRequiresPairedKeysAndCancelsOnFocusLoss) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* button = manager.GetRoot().CreateChild<UIButton>("Button");
    auto* checkbox = manager.GetRoot().CreateChild<UICheckBox>("Checkbox");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_FALSE(checkbox->IsChecked());
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_TRUE(checkbox->IsChecked());
    EXPECT_EQ(clicks, 1);
}

TEST(KeyboardNavigation, ArrowsAdjustSliderAndEditUtf8AtCursor) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* slider = manager.GetRoot().CreateChild<UISlider>("Slider");
    slider->SetMinMaxValues(0.0f, 100.0f);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_FLOAT_EQ(slider->GetValue(), 1.0f);
    manager.InjectKeyEvent(ScanCode::End, true);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_FLOAT_EQ(slider->GetValue(), 100.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'a');
    manager.InjectCharacter(U'\u00E9');
    manager.InjectCharacter(U'\U0001F600');
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(input->GetText(), "a\xF0\x9F\x98\x80");
    manager.InjectKeyEvent(ScanCode::Delete, true);
    EXPECT_EQ(input->GetText(), "a");
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectCharacter(U'b');
    EXPECT_EQ(input->GetText(), "ba");
    EXPECT_EQ(manager.GetFocusedWidget(), input);
}

TEST(KeyboardNavigation, DropdownAndTabsKeepCompositeFocus) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("Dropdown");
    dropdown->SetOptions({"First", "Second", "Third"});
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>("Tabs");
    auto* firstInput = tabs->AddTab("First")->CreateChild<UIInputBox>("FirstInput");
    auto* secondInput = tabs->AddTab("Second")->CreateChild<UIInputBox>("SecondInput");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(dropdown->GetSelectedIndex(), 1);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_TRUE(dropdown->IsExpanded());
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_FALSE(dropdown->IsExpanded());
    EXPECT_EQ(manager.GetFocusedWidget(), tabs);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), secondInput);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    EXPECT_FALSE(firstInput->IsFocused());
}

TEST(KeyboardNavigation, TreeArrowsSelectExpandCollapseAndRevealRows) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetSize(200.0f, 48.0f);
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    const auto parent = tree->AddNode(0, "Parent");
    const auto child = tree->AddNode(parent, "Child");
    const auto last = tree->AddNode(0, "Last");
    manager.Update(0.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(tree->GetSelectedNode(), parent);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(tree->GetSelectedNode(), parent);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_FALSE(tree->IsExpanded(parent));
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_TRUE(tree->IsExpanded(parent));
    manager.InjectKeyEvent(ScanCode::End, true);
    EXPECT_EQ(tree->GetSelectedNode(), last);
    EXPECT_GT(tree->GetScrollY(), 0.0f);
}

TEST(KeyboardNavigation, TabRevealsOffscreenControls) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>("Scroll");
    scroll->SetSize(100.0f, 50.0f);
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetContentSize(100.0f, 300.0f);
    auto* button = scroll->GetContent()->CreateChild<UIButton>("Offscreen");
    button->SetSize(60.0f, 20.0f);
    button->SetPoint(AnchorPoint::TopLeft, scroll->GetContent(), AnchorPoint::TopLeft, 0.0f, 200.0f);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), button);
    EXPECT_FLOAT_EQ(scroll->GetScrollY(), 170.0f);
    EXPECT_FLOAT_EQ(button->GetComputedRect().y, 30.0f);
}

TEST(KeyboardNavigation, PopupArrowsSkipUnavailableItemsAndRestoreFocus) {
    UIManager manager(std::make_shared<NullRenderer>());
    manager.SetDisplaySize(200.0f, 60.0f);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Menu");
    int first = 0;
    int last = 0;
    menu->SetItems({{"First", [&] { ++first; }}, {"Disabled", {}, false}, {"", {}, true, true}, {"Last", [&] { ++last; }}});
    manager.InjectKeyEvent(ScanCode::Tab, true);
    menu->Open(0.0f, 0.0f);
    EXPECT_FALSE(input->IsFocused());
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_GT(menu->GetScrollY(), 0.0f);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(last, 1);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    menu->Open(0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::Up, true);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(first, 1);
    menu->Open(0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    EXPECT_EQ(manager.GetFocusedWidget(), input);
    EXPECT_FALSE(menu->IsOpen());
}

TEST(KeyboardNavigation, MenuBarArrowsSwitchOpenMenus) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* bar = manager.GetRoot().CreateChild<UIMenuBar>(manager, "Bar");
    auto* first = bar->AddMenu("First", {{"Item", {}}});
    int clicks = 0;
    auto* second = bar->AddMenu("Second", {{"Item", [&] { ++clicks; }}});
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), bar);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_TRUE(first->IsOpen());
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_TRUE(second->IsOpen());
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(manager.GetFocusedWidget(), bar);
}

TEST(KeyboardNavigation, RadioArrowsMoveFocusAndSelectionWithinEnabledGroup) {
    UIManager manager(std::make_shared<NullRenderer>());
    auto* first = manager.GetRoot().CreateChild<UIRadioButton>("First");
    auto* disabled = manager.GetRoot().CreateChild<UIRadioButton>("Disabled");
    disabled->SetEnabled(false);
    auto* other = manager.GetRoot().CreateChild<UIRadioButton>("Other");
    other->SetGroup("Other");
    auto* last = manager.GetRoot().CreateChild<UIRadioButton>("Last");
    first->SetChecked(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(manager.GetFocusedWidget(), last);
    EXPECT_TRUE(last->IsChecked());
    EXPECT_FALSE(first->IsChecked());
    EXPECT_FALSE(other->IsChecked());
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(manager.GetFocusedWidget(), first);
    EXPECT_TRUE(first->IsChecked());
    EXPECT_FALSE(last->IsChecked());
}

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
