#include "LambUI/LambUI.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

using namespace LambUI;

namespace {
constexpr const char* TAG = "FontTestMeasurer";

class RecordingRenderer : public IRenderer {
public:
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& commands) override { bucket = commands; }
    std::vector<UIRenderCommand> bucket;
};

class FontTestMeasurer final : public ITextMeasurer {
public:
    FontTestMeasurer() { LAMBUI_LOGT(TAG, "Construct"); }
    ~FontTestMeasurer() override { LAMBUI_LOGT(TAG, "Destroy"); }

    void MeasureText(const std::string& text, void* fontHandle,
                     float& width, float& height) const override {
        const float advance = fontHandle ? *static_cast<const float*>(fontHandle) : 6.0f;
        width = static_cast<float>(text.size()) * advance;
        height = advance * 2.0f;
    }
};
}

TEST(FontSelection, RegistrationPreservesDefaultAndRejectsDuplicateHandles) {
    FontAtlas defaultFont;
    FontAtlas alternateFont;
    FontAtlas unknownFont;
    FontAtlasTextMeasurer measurer(defaultFont);
    EXPECT_FALSE(measurer.RegisterFont(nullptr, alternateFont));
    EXPECT_TRUE(measurer.RegisterFont(&alternateFont, alternateFont));
    EXPECT_FALSE(measurer.RegisterFont(&alternateFont, unknownFont));
    EXPECT_EQ(&measurer.GetFont(nullptr), &defaultFont);
    EXPECT_EQ(&measurer.GetFont(&unknownFont), &defaultFont);
    EXPECT_EQ(&measurer.GetFont(&alternateFont), &alternateFont);
}

TEST(FontSelection, FontAndMeasurerChangesRemeasureExistingText) {
    FontTestMeasurer measurer;
    float alternateAdvance = 10.0f;
    UIManager manager(nullptr);
    auto* label = manager.GetRoot().CreateChild<UITextWidget>("FontLabel");
    label->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    label->SetText("Existing text");
    label->SetSize(123, 45);
    label->SetTextMeasurer(&measurer);
    manager.Update(0);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 78);
    EXPECT_FLOAT_EQ(label->GetComputedRect().height, 12);
    label->SetSize(123, 45);
    label->SetFont(&alternateAdvance);
    manager.Update(0);
    EXPECT_EQ(label->GetFont(), &alternateAdvance);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 130);
    EXPECT_FLOAT_EQ(label->GetComputedRect().height, 20);
    label->SetText("New");
    manager.Update(0);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 30);
    label->SetFont(nullptr);
    manager.Update(0);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 18);
    EXPECT_FLOAT_EQ(label->GetComputedRect().height, 12);
    label->SetTextMeasurer(nullptr);
    label->SetSize(123, 45);
    label->SetFont(nullptr);
    manager.Update(0);
    EXPECT_EQ(label->GetFont(), nullptr);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 123);
}

TEST(FontSelection, MixedTextCommandsPreserveIndependentFontHandlesAndMetrics) {
    FontTestMeasurer measurer;
    float alternateAdvance = 10.0f;
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* body = manager.GetRoot().CreateChild<UITextWidget>("Body");
    auto* heading = manager.GetRoot().CreateChild<UITextWidget>("Heading");
    body->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    heading->SetPoint(AnchorPoint::TopLeft, body, AnchorPoint::BottomLeft);
    body->SetTextMeasurer(&measurer);
    heading->SetFont(&alternateAdvance);
    heading->SetText("Same");
    heading->SetTextMeasurer(&measurer);
    body->SetText("Same");
    manager.Update(0);
    manager.Render();
    const auto& commands = renderer->bucket;
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_EQ(commands[0].type, RenderCommandType::DrawString);
    EXPECT_EQ(commands[1].type, RenderCommandType::DrawString);
    EXPECT_EQ(commands[0].fontHandle, nullptr);
    EXPECT_EQ(commands[1].fontHandle, &alternateAdvance);
    EXPECT_FLOAT_EQ(commands[0].width, 24);
    EXPECT_FLOAT_EQ(commands[1].width, 40);
    EXPECT_FLOAT_EQ(commands[1].y, 12);
}

TEST(FontSelection, InputBoxPreservesFontThroughEditingAndFixedSize) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    FontAtlas font;
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    input->SetSize(180, 30);
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetFont(&font);
    input->SetText("A");
    manager.Update(0);
    manager.InjectMouseMove(10, 10);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectCharacter(U'B');
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[2].fontHandle, &font);
    EXPECT_EQ(renderer->bucket[2].text, "AB");
    EXPECT_EQ(input->GetFont(), &font);
    EXPECT_FLOAT_EQ(input->GetComputedRect().width, 180);
    input->SetFont(nullptr);
    manager.Update(0);
    manager.Render();
    EXPECT_EQ(renderer->bucket[2].fontHandle, nullptr);
}

TEST(InputCaret, BlinksUsingInjectedTimeAndResetsOnEditingAndFocus) {
    auto renderer = std::make_shared<RecordingRenderer>();
    auto measurer = std::make_shared<FontTestMeasurer>();
    UIManager manager(renderer, measurer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    input->SetSize(180, 30);
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10, 20);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 4u);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[3].type, RenderCommandType::DrawQuad);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 14);
    EXPECT_FLOAT_EQ(renderer->bucket[3].y, 24);
    EXPECT_FLOAT_EQ(renderer->bucket[3].height, 12);
    manager.Update(0.25f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 5u);
    manager.Update(0.25f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 4u);
    manager.Update(0.5f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 5u);
    manager.Update(2.5f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 4u);
    manager.InjectCharacter(U'A');
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 20);
    manager.Update(0.5f);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 14);
    input->SetKeyboardEnabled(false);
    manager.Update(0);
    manager.Render();
    EXPECT_FALSE(input->IsFocused());
    EXPECT_EQ(renderer->bucket.size(), 4u);
    input->SetKeyboardEnabled(true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.Render();
    EXPECT_TRUE(input->IsFocused());
    EXPECT_EQ(renderer->bucket.size(), 5u);
}

TEST(InputCaret, UsesSelectedFontAndKeepsLongTextInsertionPointClipped) {
    auto renderer = std::make_shared<RecordingRenderer>();
    auto measurer = std::make_shared<FontTestMeasurer>();
    UIManager manager(renderer, measurer);
    float alternateAdvance = 10.0f;
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    input->SetSize(40, 24);
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10, 20);
    input->SetFont(&alternateAdvance);
    input->SetText("abcdef");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[1].type, RenderCommandType::PushScissor);
    EXPECT_FLOAT_EQ(renderer->bucket[1].x, 14);
    EXPECT_FLOAT_EQ(renderer->bucket[1].width, 32);
    EXPECT_FLOAT_EQ(renderer->bucket[2].x, -15);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 45);
    EXPECT_FLOAT_EQ(renderer->bucket[3].width, 1);
    EXPECT_FLOAT_EQ(renderer->bucket[3].height, 16);
    EXPECT_EQ(renderer->bucket[4].type, RenderCommandType::PopScissor);
    manager.Update(0.5f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 4u);
    EXPECT_FLOAT_EQ(renderer->bucket[2].x, -15);
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[2].x, 14);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 24);
    input->SetFont(nullptr);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 20);
    EXPECT_FLOAT_EQ(renderer->bucket[3].height, 12);
    manager.Update(0.5f);
    manager.InjectKeyEvent(ScanCode::Delete, true);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(input->GetText(), "acdef");
    manager.Update(0.5f);
    input->Backspace();
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 14);
    manager.Update(0.5f);
    input->SetText("ab");
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 26);
}

TEST(InputCaret, FallbackCountsUtf8CodepointsAndRejectsInvalidFrameTimes) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>("Input");
    input->SetSize(180, 30);
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectCharacter(U'A');
    manager.InjectCharacter(U'\u00e9');
    manager.InjectCharacter(U'\U0001f600');
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 28);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 20);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket[3].x, 12);
    manager.Update(-1.0f);
    manager.Update(std::numeric_limits<float>::quiet_NaN());
    manager.Update(std::numeric_limits<float>::infinity());
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 5u);
    manager.Update(0.5f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 4u);
}

TEST(InputCaret, TinyFieldsAndHiddenAncestorsCannotLeakCaret) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* parent = manager.GetRoot().CreateChild<UIWidget>("Parent");
    parent->SetAllPoints(&manager.GetRoot());
    auto* input = parent->CreateChild<UIInputBox>("Input");
    input->SetSize(8, 30);
    input->SetPoint(AnchorPoint::TopLeft, parent, AnchorPoint::TopLeft);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 1u);
    input->SetSize(8.5f, 8.5f);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[3].width, 0.5f);
    EXPECT_FLOAT_EQ(renderer->bucket[3].height, 0.5f);
    parent->SetVisible(false);
    manager.Update(0);
    manager.Render();
    EXPECT_TRUE(renderer->bucket.empty());
    EXPECT_FALSE(input->IsFocused());
    parent->SetVisible(true);
    manager.Update(0);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 4u);
}

TEST(TextWrapping, ExplicitBreaksMeasureAndRenderBlankLines) {
    FontTestMeasurer measurer;
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* label = manager.GetRoot().CreateChild<UITextWidget>();
    label->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    label->SetTextMeasurer(&measurer);
    label->SetText("one\r\ntwo\n\n");
    manager.Update(0);
    manager.Render();
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 18);
    EXPECT_FLOAT_EQ(label->GetComputedRect().height, 48);
    ASSERT_EQ(renderer->bucket.size(), 4u);
    EXPECT_EQ(renderer->bucket[0].text, "one");
    EXPECT_EQ(renderer->bucket[1].text, "two");
    EXPECT_EQ(renderer->bucket[2].text, "");
    EXPECT_EQ(renderer->bucket[3].text, "");
    EXPECT_FLOAT_EQ(renderer->bucket[3].y, 36);
}

TEST(TextWrapping, WrapsWordsReflowsWithBoundsAndPreservesUtf8) {
    FontTestMeasurer measurer;
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* label = manager.GetRoot().CreateChild<UITextWidget>();
    label->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    label->SetWordWrap(true);
    label->SetTextMeasurer(&measurer);
    label->SetSize(24, 80);
    label->SetText("one two");
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 4u);
    EXPECT_EQ(renderer->bucket.front().type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->bucket[1].text, "one ");
    EXPECT_EQ(renderer->bucket[2].text, "two");
    EXPECT_FLOAT_EQ(renderer->bucket[2].y, 12);
    EXPECT_EQ(renderer->bucket.back().type, RenderCommandType::PopScissor);
    EXPECT_FLOAT_EQ(label->GetComputedRect().width, 24);
    label->SetSize(60, 80);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 3u);
    EXPECT_EQ(renderer->bucket[1].text, "one two");
    label->SetTextMeasurer(nullptr);
    label->SetText("\xc3\xa9\xf0\x9f\x98\x80!");
    label->SetSize(8, 80);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[1].text, "\xc3\xa9");
    EXPECT_EQ(renderer->bucket[2].text, "\xf0\x9f\x98\x80");
    EXPECT_EQ(renderer->bucket[3].text, "!");
    label->SetSize(0, 0);
    manager.Update(0);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 5u);
    EXPECT_FLOAT_EQ(renderer->bucket[0].width, 0);
}

TEST(MultilineInput, EnterEditsAndDeletionJoinsLinesWithoutSubmitting) {
    UIManager manager(nullptr);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetSize(100, 80);
    input->SetMultiline(true);
    input->SetText("ab\r\ncd\r");
    EXPECT_EQ(input->GetText(), "ab\ncd\n");
    int changes = 0, submissions = 0;
    input->RegisterCallback(UIEventType::OnTextChanged, [&](const UIEventData&) { ++changes; });
    input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(input->GetText(), "abcd");
    EXPECT_EQ(input->GetCursorPosition(), 2u);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectCharacter(U'\n');
    EXPECT_EQ(input->GetText(), "ab\ncd");
    EXPECT_EQ(changes, 3);
    EXPECT_EQ(submissions, 0);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.InjectKeyEvent(ScanCode::Delete, true);
    EXPECT_EQ(input->GetText(), "abcd");
    input->SetText("ab\ncd");
    input->SetMultiline(false);
    EXPECT_EQ(input->GetText(), "ab cd");
    manager.InjectKeyEvent(ScanCode::Enter, true);
    EXPECT_EQ(submissions, 1);
    EXPECT_EQ(changes, 4);
}

TEST(MultilineInput, VerticalNavigationRetainsColumnAcrossShortLinesAndUtf8) {
    UIManager manager(nullptr);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetSize(200, 100);
    input->SetMultiline(true);
    input->SetText("abcd\nx\n\xc3\xa9" "bcd");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_EQ(input->GetCursorPosition(), 6u);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_EQ(input->GetCursorPosition(), 4u);
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    manager.InjectKeyEvent(ScanCode::Down, true);
    EXPECT_EQ(input->GetCursorPosition(), 9u);
    manager.InjectCharacter(U'!');
    EXPECT_EQ(input->GetText(), "abcd\nx\n\xc3\xa9!bcd");
}

TEST(MultilineInput, SoftLineNavigationAndCaretStayInsideScrolledViewport) {
    auto renderer = std::make_shared<RecordingRenderer>();
    auto measurer = std::make_shared<FontTestMeasurer>();
    UIManager manager(renderer, measurer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetMultiline(true);
    input->SetSize(33, 28);
    input->SetText("abcdefghij");
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 6u);
    EXPECT_EQ(renderer->bucket[2].text, "abcd");
    EXPECT_EQ(renderer->bucket[3].text, "efgh");
    EXPECT_EQ(renderer->bucket[4].text, "ij");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 7u);
    EXPECT_FLOAT_EQ(renderer->bucket[5].x, 16);
    EXPECT_FLOAT_EQ(renderer->bucket[5].y, 12);
    EXPECT_FLOAT_EQ(renderer->bucket[5].height, 12);
    EXPECT_FLOAT_EQ(renderer->bucket[2].y, -12);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_EQ(input->GetCursorPosition(), 6u);
    manager.InjectKeyEvent(ScanCode::End, true);
    EXPECT_EQ(input->GetCursorPosition(), 8u);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket[5].x, 28);
    manager.InjectKeyEvent(ScanCode::Home, true);
    EXPECT_EQ(input->GetCursorPosition(), 4u);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_EQ(input->GetCursorPosition(), 0u);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket[5].y, 4);
    input->SetWordWrap(false);
    input->SetText("abcdef\n");
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 6u);
    EXPECT_EQ(renderer->bucket[2].text, "abcdef");
    EXPECT_EQ(renderer->bucket[3].text, "");
    EXPECT_FLOAT_EQ(renderer->bucket[4].x, 4);
}

TEST(TextWrapping, BoundarySpacesStayWithPreviousLineWithoutShiftingCaret) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetSize(33, 60);
    input->SetMultiline(true);
    input->SetText("one   two");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.InjectKeyEvent(ScanCode::Up, true);
    manager.InjectKeyEvent(ScanCode::End, true);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 6u);
    EXPECT_EQ(renderer->bucket[2].text, "one   ");
    EXPECT_EQ(renderer->bucket[3].text, "two");
    EXPECT_FLOAT_EQ(renderer->bucket[2].x, 4);
    EXPECT_FLOAT_EQ(renderer->bucket[4].x, 28);
    EXPECT_FLOAT_EQ(renderer->bucket[4].y, 4);
    EXPECT_EQ(input->GetCursorPosition(), 6u);
    EXPECT_EQ(input->GetText(), "one   two");
}

TEST(MultilineInput, FontPaddingAndResizingReflowWithoutFocus) {
    auto renderer = std::make_shared<RecordingRenderer>();
    auto measurer = std::make_shared<FontTestMeasurer>();
    UIManager manager(renderer, measurer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetMultiline(true);
    input->SetSize(37, 80);
    input->SetPadding({2, 3, 2, 3});
    input->SetText("abcdef");
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[2].text, "abcd");
    EXPECT_FLOAT_EQ(renderer->bucket[2].x, 6);
    EXPECT_FLOAT_EQ(renderer->bucket[2].y, 7);
    float alternateAdvance = 10.0f;
    input->SetFont(&alternateAdvance);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 6u);
    EXPECT_EQ(renderer->bucket[2].text, "ab");
    EXPECT_EQ(renderer->bucket[2].fontHandle, &alternateAdvance);
    EXPECT_FLOAT_EQ(renderer->bucket[3].y, 27);
    input->SetSize(73, 80);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 4u);
    EXPECT_EQ(renderer->bucket[2].text, "abcdef");
}

TEST(TextSelection, ShiftNavigationReplacesUtf8AndDisablingPreventsSelection) {
    UIManager manager(nullptr);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetSize(200, 80);
    input->SetText("a\xc3\xa9" "bc");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    int changes = 0;
    input->RegisterCallback(UIEventType::OnTextChanged, [&](const UIEventData&) { ++changes; });
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.InjectKeyEvent(ScanCode::Left, true);
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(input->GetSelectedText(), "\xc3\xa9" "bc");
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectCharacter(U'!');
    EXPECT_EQ(input->GetText(), "a!");
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(input->GetSelectedText(), "");
    manager.InjectKeyEvent(ScanCode::LeftControl, true);
    manager.InjectKeyEvent(ScanCode::A, true);
    manager.InjectKeyEvent(ScanCode::LeftControl, false);
    EXPECT_EQ(input->GetSelectedText(), "a!");
    input->SetSelectionEnabled(false);
    EXPECT_FALSE(input->IsSelectionEnabled());
    EXPECT_EQ(input->GetSelectedText(), "");
    input->SelectAll();
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    EXPECT_EQ(input->GetSelectedText(), "");
    manager.InjectCharacter(U'X');
    EXPECT_EQ(input->GetText(), "Xa!");
}

TEST(TextSelection, MultilineRangeHighlightsAndDeletesWithOneNotification) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetSize(120, 80);
    input->SetMultiline(true);
    input->SetText("ab\n\ncd");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    input->SetSelection(1, 5);
    manager.Update(0);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 10u);
    EXPECT_EQ(renderer->bucket[1].type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->bucket[2].type, RenderCommandType::DrawQuad);
    EXPECT_EQ(renderer->bucket[3].type, RenderCommandType::DrawQuad);
    EXPECT_FLOAT_EQ(renderer->bucket[3].width, 4);
    EXPECT_FLOAT_EQ(renderer->bucket[4].y, 36);
    EXPECT_EQ(renderer->bucket[5].type, RenderCommandType::DrawString);
    EXPECT_EQ(input->GetSelectedText(), "b\n\nc");
    int changes = 0;
    input->RegisterCallback(UIEventType::OnTextChanged, [&](const UIEventData&) { ++changes; });
    manager.InjectKeyEvent(ScanCode::Delete, true);
    EXPECT_EQ(input->GetText(), "ad");
    EXPECT_EQ(changes, 1);
    input->SetSelection(2, 0);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    EXPECT_EQ(input->GetText(), "\n");
    EXPECT_EQ(changes, 2);
    input->SelectAll();
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(input->GetText(), "");
    EXPECT_EQ(changes, 3);
}

TEST(TextSelection, MouseDragAndShiftClickUseMeasuredLinesAndCapture) {
    auto renderer = std::make_shared<RecordingRenderer>();
    auto measurer = std::make_shared<FontTestMeasurer>();
    UIManager manager(renderer, measurer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetSize(60, 50);
    input->SetPadding({2, 2, 2, 2});
    input->SetMultiline(true);
    input->SetText("abcd\nefgh");
    manager.Update(0);
    manager.Render();
    manager.InjectMouseMove(12, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    EXPECT_EQ(input->GetCursorPosition(), 1u);
    manager.InjectMouseMove(18, 20);
    EXPECT_EQ(input->GetSelectedText(), "bcd\nef");
    manager.InjectMouseButton(MouseButton::Right, false);
    manager.InjectMouseMove(200, 200);
    EXPECT_EQ(input->GetSelectedText(), "bcd\nefgh");
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseMove(6, 8);
    EXPECT_EQ(input->GetSelectedText(), "bcd\nefgh");
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    EXPECT_EQ(input->GetSelectedText(), "a");
    input->SetSelectionEnabled(false);
    manager.InjectMouseMove(12, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(200, 200);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(input->GetCursorPosition(), 1u);
    EXPECT_EQ(input->GetSelectedText(), "");
}

TEST(TextSelection, MouseUsesVisibleScrollOffsetAndUtf8Boundaries) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetSize(33, 40);
    input->SetText("abcd\xc3\xa9");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.Update(0);
    manager.Render();
    manager.InjectMouseMove(4, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    EXPECT_EQ(input->GetCursorPosition(), 2u);
    manager.InjectMouseMove(28, 8);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(input->GetSelectedText(), "cd\xc3\xa9");
    input->SetSelection(5, 1000);
    EXPECT_EQ(input->GetSelectionStart(), 4u);
    EXPECT_EQ(input->GetSelectionEnd(), 6u);
    EXPECT_EQ(input->GetSelectedText(), "\xc3\xa9");
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(input->GetCursorPosition(), 4u);
    EXPECT_EQ(input->GetSelectedText(), "");
}

TEST(TextSelection, WrappedShiftNavigationAndHeldModifiersSurviveFocusChange) {
    UIManager manager(nullptr);
    auto* first = manager.GetRoot().CreateChild<UIInputBox>();
    auto* second = manager.GetRoot().CreateChild<UIInputBox>();
    first->SetSize(41, 80);
    second->SetSize(41, 80);
    second->SetMultiline(true);
    second->SetText("abcdefghij");
    manager.InjectKeyEvent(ScanCode::Tab, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::RightShift, true);
    manager.InjectKeyEvent(ScanCode::Tab, true);
    EXPECT_EQ(manager.GetFocusedWidget(), second);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::Up, true);
    EXPECT_EQ(second->GetSelectedText(), "ghij");
    manager.InjectKeyEvent(ScanCode::Home, true);
    EXPECT_EQ(second->GetSelectedText(), "efghij");
    manager.InjectKeyEvent(ScanCode::RightShift, false);
    manager.InjectKeyEvent(ScanCode::Right, true);
    EXPECT_EQ(second->GetCursorPosition(), 10u);
    EXPECT_EQ(second->GetSelectedText(), "");
    manager.InjectKeyEvent(ScanCode::LeftControl, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Home, true);
    EXPECT_EQ(second->GetSelectedText(), "abcdefghij");
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    manager.InjectKeyEvent(ScanCode::LeftControl, false);
}

TEST(TextSelection, DragAutoscrollUsesInjectedTimeAndStopsOnFocusLoss) {
    auto renderer = std::make_shared<RecordingRenderer>();
    UIManager manager(renderer);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetMultiline(true);
    input->SetSize(80, 40);
    input->SetText("a\nb\nc\nd\ne\nf");
    manager.Update(0);
    manager.Render();
    manager.InjectMouseMove(4, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(12, 42);
    const size_t initial = input->GetCursorPosition();
    manager.Update(0.1f);
    EXPECT_GT(input->GetCursorPosition(), initial);
    manager.Render();
    size_t highlights = 0;
    for (const auto& command : renderer->bucket)
        if (command.type == RenderCommandType::DrawQuad && command.color == 0x287EA8FFu) ++highlights;
    EXPECT_GT(highlights, 1u);
    input->SetKeyboardEnabled(false);
    manager.Update(0);
    const size_t stopped = input->GetCursorPosition();
    manager.Update(0.1f);
    manager.InjectMouseMove(12, 1000);
    EXPECT_EQ(input->GetCursorPosition(), stopped);
}

TEST(InputEditing, ReadOnlyBlocksUserEditsButPreservesSelectionAndSubmission) {
    for (bool multiline : {false, true}) {
        UIManager manager(nullptr);
        auto* input = manager.GetRoot().CreateChild<UIInputBox>();
        input->SetSize(160, 80);
        input->SetMultiline(multiline);
        input->SetText(multiline ? "first\nsecond" : "single line");
        const std::string original = input->GetText();
        EXPECT_TRUE(input->IsEditingEnabled());
        manager.InjectKeyEvent(ScanCode::Tab, true);
        int changes = 0, submissions = 0;
        input->RegisterCallback(UIEventType::OnTextChanged, [&](const UIEventData&) { ++changes; });
        input->RegisterCallback(UIEventType::OnEnterPressed, [&](const UIEventData&) { ++submissions; });
        input->SelectAll();
        input->SetEditingEnabled(false);
        EXPECT_FALSE(input->IsEditingEnabled());
        EXPECT_TRUE(input->IsSelectionEnabled());
        manager.InjectCharacter(U'X');
        manager.InjectKeyEvent(ScanCode::Backspace, true);
        manager.InjectKeyEvent(ScanCode::Delete, true);
        manager.InjectKeyEvent(ScanCode::Enter, true);
        input->AppendCharacter(U'Y');
        input->Backspace();
        input->SubmitEnter();
        EXPECT_EQ(input->GetText(), original);
        EXPECT_EQ(input->GetSelectedText(), original);
        EXPECT_EQ(changes, 0);
        EXPECT_EQ(submissions, multiline ? 0 : 2);
        EXPECT_EQ(manager.GetFocusedWidget(), input);
        input->SetEditingEnabled(true);
        manager.InjectCharacter(U'!');
        EXPECT_EQ(input->GetText(), "!");
        EXPECT_EQ(input->GetSelectedText(), "");
        EXPECT_EQ(changes, 1);
    }
}

TEST(InputEditing, ReadOnlyAllowsNavigationSelectionAndProgrammaticText) {
    UIManager manager(nullptr);
    auto* input = manager.GetRoot().CreateChild<UIInputBox>();
    input->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    input->SetSize(160, 80);
    input->SetText("abcd");
    input->SetEditingEnabled(false);
    manager.Update(0);
    manager.InjectMouseMove(12, 8);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(28, 8);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_EQ(input->GetSelectedText(), "bc");
    manager.InjectKeyEvent(ScanCode::Left, true);
    EXPECT_EQ(input->GetCursorPosition(), 1u);
    manager.InjectKeyEvent(ScanCode::LeftShift, true);
    manager.InjectKeyEvent(ScanCode::Right, true);
    manager.InjectKeyEvent(ScanCode::LeftShift, false);
    EXPECT_EQ(input->GetSelectedText(), "b");
    input->SetSelectionEnabled(false);
    manager.InjectKeyEvent(ScanCode::Home, true);
    manager.InjectKeyEvent(ScanCode::Delete, true);
    EXPECT_EQ(input->GetText(), "abcd");
    EXPECT_EQ(input->GetCursorPosition(), 0u);
    EXPECT_EQ(input->GetSelectedText(), "");
    input->SetText("updated");
    EXPECT_EQ(input->GetText(), "updated");
    EXPECT_FALSE(input->IsEditingEnabled());
    input->SetEditingEnabled(true);
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(input->GetText(), "update");
    EXPECT_FALSE(input->IsSelectionEnabled());
}

TEST(WindowWidget, InjectedMoveResizeLimitsAndLocks) {
    UIManager manager(nullptr);
    manager.SetDisplaySize(800, 600);
    auto* window = manager.GetRoot().CreateChild<UIWindow>("Window");
    window->SetBounds(50, 60, 300, 200);
    window->SetSizeLimits(200, 120, 400, 300);
    manager.Update(0);
    const auto drag = [&](float x, float y, float dx, float dy) {
        manager.InjectMouseMove(x, y);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseMove(x + dx, y + dy);
        manager.Update(0);
        manager.InjectMouseButton(MouseButton::Left, false);
    };
    drag(70, 75, 30, 40);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 80);
    EXPECT_FLOAT_EQ(window->GetComputedRect().y, 100);
    window->SetMovable(false);
    drag(100, 115, 30, 40);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 80);
    drag(379, 299, 200, 200);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, 400);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 300);
    drag(81, 101, 500, 500);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, 200);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 120);
    const auto locked = window->GetComputedRect();
    window->SetResizable(false);
    drag(locked.x + locked.width - 1, locked.y + locked.height - 1, 80, 80);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, locked.width);
}

TEST(WindowWidget, PendingBoundsSurviveStateTransitionsAndSizeLimits) {
    UIManager manager(nullptr);
    auto* window = manager.GetRoot().CreateChild<UIWindow>();
    window->SetBounds(20, 30, 300, 200);
    manager.Update(0);
    window->SetBounds(50, 60, 400, 300);
    window->Minimize();
    window->Restore();
    window->Maximize();
    window->Restore();
    window->SetSizeLimits(180, 100, 350, 250);
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 50);
    EXPECT_FLOAT_EQ(window->GetComputedRect().y, 60);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, 350);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 250);
    manager.InjectMouseMove(80, 76);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(100, 96);
    manager.Update(0);
    manager.InjectMouseMove(80, 76);
    manager.Update(0);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 50);
    EXPECT_FLOAT_EQ(window->GetComputedRect().y, 60);
}

TEST(WindowWidget, TitleButtonsStatesVisibilityAndParentResize) {
    UIManager manager(nullptr);
    manager.SetDisplaySize(800, 600);
    auto* window = manager.GetRoot().CreateChild<UIWindow>("Window");
    window->SetBounds(50, 60, 300, 200);
    manager.Update(0);
    const auto click = [&](WindowButton button) {
        const auto rect = window->GetButtonRect(button);
        manager.InjectMouseMove(rect.x + 10, rect.y + 10);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseButton(MouseButton::Left, false);
        manager.Update(0);
    };
    window->SetButtonMode(WindowButton::Close, WindowButtonMode::Disabled);
    click(WindowButton::Close);
    EXPECT_TRUE(window->IsVisible());
    window->SetButtonMode(WindowButton::Close, WindowButtonMode::Hidden);
    EXPECT_FLOAT_EQ(window->GetButtonRect(WindowButton::Close).width, 0);
    click(WindowButton::Minimize);
    EXPECT_EQ(window->GetWindowState(), WindowState::Minimized);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 32);
    EXPECT_FALSE(window->GetContent()->GetParent()->IsVisible());
    click(WindowButton::Minimize);
    EXPECT_EQ(window->GetWindowState(), WindowState::Normal);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 200);
    click(WindowButton::Maximize);
    EXPECT_EQ(window->GetWindowState(), WindowState::Maximized);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, 800);
    manager.SetDisplaySize(640, 480);
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().width, 640);
    click(WindowButton::Minimize);
    click(WindowButton::Minimize);
    EXPECT_EQ(window->GetWindowState(), WindowState::Maximized);
    click(WindowButton::Maximize);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 50);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 200);
    window->SetButtonMode(WindowButton::Close, WindowButtonMode::Enabled);
    click(WindowButton::Close);
    EXPECT_FALSE(window->IsVisible());
    window->SetVisible(true);
    EXPECT_TRUE(window->IsVisible());
}

TEST(WindowWidget, ContentClickRaisesWindowWithoutMovingIt) {
    UIManager manager(nullptr);
    auto* back = manager.GetRoot().CreateChild<UIWindow>("Back");
    back->SetBounds(0, 0, 300, 200);
    auto* button = back->GetContent()->CreateChild<UIButton>();
    button->SetAllPoints(back->GetContent());
    auto* front = manager.GetRoot().CreateChild<UIWindow>("Front");
    front->SetBounds(150, 0, 300, 200);
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0);
    for (float x : {50.0f, 200.0f}) {
        manager.InjectMouseMove(x, 60);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseButton(MouseButton::Left, false);
    }
    EXPECT_EQ(clicks, 2);
    EXPECT_FLOAT_EQ(back->GetComputedRect().x, 0);
}

TEST(WindowWidget, EachTitleButtonCanBeDisabledOrHidden) {
    UIManager manager(nullptr);
    auto* window = manager.GetRoot().CreateChild<UIWindow>();
    window->SetBounds(20, 20, 300, 200);
    manager.Update(0);
    for (auto button : {WindowButton::Minimize, WindowButton::Maximize, WindowButton::Close}) {
        window->SetButtonMode(button, WindowButtonMode::Disabled);
        const auto rect = window->GetButtonRect(button);
        manager.InjectMouseMove(rect.x + 10, rect.y + 10);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseButton(MouseButton::Left, false);
        EXPECT_EQ(window->GetWindowState(), WindowState::Normal);
        EXPECT_TRUE(window->IsVisible());
        window->SetButtonMode(button, WindowButtonMode::Hidden);
        EXPECT_FLOAT_EQ(window->GetButtonRect(button).width, 0);
    }
}

TEST(WindowWidget, CapturePairsButtonsCancelsLocksAndIgnoresHiddenWindow) {
    UIManager manager(nullptr);
    auto* window = manager.GetRoot().CreateChild<UIWindow>();
    window->SetBounds(20, 20, 300, 200);
    manager.Update(0);
    manager.InjectMouseMove(60, 36);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Right, false);
    manager.InjectMouseMove(80, 46);
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 40);
    window->SetMovable(false);
    manager.InjectMouseMove(120, 80);
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 40);
    manager.InjectMouseButton(MouseButton::Left, false);
    window->SetMovable(true);
    manager.InjectMouseMove(80, 46);
    manager.InjectMouseButton(MouseButton::Left, true);
    window->SetVisible(false);
    manager.InjectMouseMove(140, 90);
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 40);
    manager.InjectMouseButton(MouseButton::Left, false);
}

TEST(WindowWidget, MinimizedMoveRestoresNewPositionAndEmitsDedicatedEvents) {
    UIManager manager(nullptr);
    auto* window = manager.GetRoot().CreateChild<UIWindow>();
    window->SetBounds(20, 20, 300, 200);
    int states = 0, closes = 0;
    window->RegisterCallback(UIEventType::OnWindowStateChanged, [&](const UIEventData&) { ++states; });
    window->RegisterCallback(UIEventType::OnClose, [&](const UIEventData&) { ++closes; });
    manager.Update(0);
    window->Minimize();
    manager.Update(0);
    manager.InjectMouseMove(60, 36);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseMove(90, 56);
    manager.Update(0);
    manager.InjectMouseButton(MouseButton::Left, false);
    window->Restore();
    manager.Update(0);
    EXPECT_FLOAT_EQ(window->GetComputedRect().x, 50);
    EXPECT_FLOAT_EQ(window->GetComputedRect().y, 40);
    EXPECT_FLOAT_EQ(window->GetComputedRect().height, 200);
    EXPECT_EQ(states, 2);
    window->Close();
    window->Close();
    EXPECT_EQ(closes, 1);
}

TEST(SelectionControls, CheckboxMouseKeyboardAndDisabledState) {
    UIManager manager(nullptr);
    auto* checkbox = manager.GetRoot().CreateChild<UICheckBox>();
    checkbox->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    int changes = 0;
    checkbox->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    manager.Update(0.0f);
    manager.InjectMouseMove(40.0f, 10.0f);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_TRUE(checkbox->IsChecked());
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Space, true);
    manager.InjectKeyEvent(ScanCode::Space, false);
    EXPECT_FALSE(checkbox->IsChecked());
    EXPECT_EQ(changes, 2);
    checkbox->SetEnabled(false);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectKeyEvent(ScanCode::Enter, true);
    manager.InjectKeyEvent(ScanCode::Enter, false);
    EXPECT_FALSE(checkbox->IsChecked());
    EXPECT_EQ(changes, 2);
    checkbox->SetChecked(true);
    checkbox->SetChecked(true);
    EXPECT_EQ(changes, 3);
}

TEST(SelectionControls, RadioGroupsAreExclusiveScopedAndNotifyAfterSelection) {
    UIManager manager(nullptr);
    auto* first = manager.GetRoot().CreateChild<UIRadioButton>();
    auto* second = manager.GetRoot().CreateChild<UIRadioButton>();
    auto* independent = manager.GetRoot().CreateChild<UIRadioButton>();
    independent->SetGroup("Other");
    auto* parent = manager.GetRoot().CreateChild<UIWidget>();
    auto* nested = parent->CreateChild<UIRadioButton>();
    first->SetChecked(true);
    independent->SetChecked(true);
    nested->SetChecked(true);
    int changes = 0;
    first->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) {
        EXPECT_FALSE(first->IsChecked());
        EXPECT_TRUE(second->IsChecked());
        ++changes;
    });
    second->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    second->SetChecked(true);
    second->SetChecked(true);
    EXPECT_EQ(changes, 2);
    EXPECT_TRUE(independent->IsChecked());
    EXPECT_TRUE(nested->IsChecked());
    second->FireEvent(UIEventData{UIEventType::OnClick});
    EXPECT_TRUE(second->IsChecked());
    EXPECT_EQ(changes, 2);
}

namespace {
class CompoundWidgets : public testing::Test {
protected:
    std::shared_ptr<RecordingRenderer> renderer = std::make_shared<RecordingRenderer>();
    UIManager manager{renderer};

    void Click(float mouseX, float mouseY, MouseButton button = MouseButton::Left) {
        manager.Update(0.0f);
        manager.InjectMouseMove(mouseX, mouseY);
        manager.InjectMouseButton(button, true);
        manager.InjectMouseButton(button, false);
    }
};
}

TEST_F(CompoundWidgets, DropdownRendersLabelsAndReusesOnlyCurrentOptions) {
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>();
    dropdown->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    dropdown->SetSize(160, 28);
    dropdown->SetOptions({"Draft", "Final", "Old"});
    dropdown->Toggle();
    manager.Update(0);
    manager.Render();
    std::vector<std::string> labels;
    for (const auto& command : renderer->bucket) if (command.type == RenderCommandType::DrawString) labels.push_back(command.text);
    EXPECT_EQ(labels, (std::vector<std::string>{"Draft", "Draft", "Final", "Old"}));
    dropdown->SetOptions({"New"});
    EXPECT_FALSE(dropdown->IsExpanded());
    dropdown->Toggle();
    manager.Update(0);
    manager.Render();
    labels.clear();
    for (const auto& command : renderer->bucket) if (command.type == RenderCommandType::DrawString) labels.push_back(command.text);
    EXPECT_EQ(labels, (std::vector<std::string>{"New", "New"}));
    manager.InjectMouseMove(10, 35);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    EXPECT_FALSE(dropdown->IsExpanded());
}

TEST_F(CompoundWidgets, ProgressBarClampsAndEmitsHorizontalAndVerticalFill) {
    auto* bar = manager.GetRoot().CreateChild<UIProgressBar>("Progress");
    bar->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft, 10.0f, 20.0f);
    bar->SetSize(200.0f, 40.0f);
    bar->SetMinMaxValues(0.0f, 100.0f);
    int changes = 0;
    bar->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    bar->SetValue(25.0f);
    manager.Update(0.0f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 2u);
    EXPECT_FLOAT_EQ(renderer->bucket.back().width, 50.0f);
    EXPECT_FALSE(bar->IsMouseEnabled());
    bar->SetOrientation(ProgressOrientation::Vertical);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket.back().height, 10.0f);
    EXPECT_FLOAT_EQ(renderer->bucket.back().y, 50.0f);
    bar->SetValue(500.0f);
    bar->SetValue(500.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(renderer->bucket.back().height, 40.0f);
    EXPECT_EQ(changes, 2);
    bar->SetValue(-1.0f);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 1u);
}

TEST_F(CompoundWidgets, ProgressBarHandlesReversedDegenerateAndNonfiniteRanges) {
    auto* bar = manager.GetRoot().CreateChild<UIProgressBar>();
    bar->SetMinMaxValues(20.0f, 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMinValue(), 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMaxValue(), 20.0f);
    EXPECT_FLOAT_EQ(bar->GetValue(), 10.0f);
    bar->SetValue(std::numeric_limits<float>::quiet_NaN());
    bar->SetMinMaxValues(0.0f, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(bar->GetValue(), 10.0f);
    EXPECT_FLOAT_EQ(bar->GetMaxValue(), 20.0f);
    bar->SetMinMaxValues(5.0f, 5.0f);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(bar->GetValue(), 5.0f);
    EXPECT_EQ(renderer->bucket.size(), 1u);
}

TEST_F(CompoundWidgets, TabsSwitchThroughInjectionAndPreserveHiddenPageState) {
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>("Tabs");
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->SetSize(200.0f, 150.0f);
    auto* first = tabs->AddTab("First");
    auto* second = tabs->AddTab("Second");
    auto* firstButton = first->CreateChild<UIButton>();
    firstButton->SetAllPoints(first);
    int clicks = 0;
    int changes = 0;
    firstButton->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    tabs->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 1);
    Click(150.0f, 10.0f, MouseButton::Right);
    EXPECT_EQ(tabs->GetSelectedIndex(), 0);
    Click(150.0f, 10.0f);
    EXPECT_EQ(tabs->GetSelectedIndex(), 1);
    EXPECT_FALSE(first->IsVisible());
    EXPECT_TRUE(second->IsVisible());
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 1);
    tabs->SetSelectedIndex(1);
    tabs->SetSelectedIndex(99);
    EXPECT_EQ(changes, 1);
    tabs->SetSize(300.0f, 200.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(second->GetComputedRect().width, 300.0f);
    EXPECT_FLOAT_EQ(second->GetComputedRect().height, 172.0f);
    Click(20.0f, 10.0f);
    Click(20.0f, 50.0f);
    EXPECT_EQ(clicks, 2);
    EXPECT_EQ(tabs->GetPage(0), first);
    EXPECT_EQ(tabs->GetPage(-1), nullptr);
}

TEST_F(CompoundWidgets, TreeExpansionSelectionScrollingAndClear) {
    auto* tree = manager.GetRoot().CreateChild<UITreeView>("Tree");
    tree->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tree->SetSize(200.0f, 48.0f);
    const auto parent = tree->AddNode(UITreeView::RootNode, "Parent");
    const auto child = tree->AddNode(parent, "Child");
    const auto grandchild = tree->AddNode(child, "Grandchild");
    tree->AddNode(UITreeView::RootNode, "Sibling");
    EXPECT_EQ(tree->AddNode(99, "Invalid"), UITreeView::RootNode);
    int changes = 0;
    tree->RegisterCallback(UIEventType::OnValueChanged, [&](const UIEventData&) { ++changes; });
    Click(60.0f, 36.0f);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    Click(5.0f, 12.0f);
    EXPECT_FALSE(tree->IsExpanded(parent));
    EXPECT_EQ(tree->GetVisibleNodeCount(), 2u);
    EXPECT_EQ(tree->GetSelectedNode(), child);
    EXPECT_EQ(changes, 1);
    Click(5.0f, 12.0f);
    EXPECT_EQ(tree->GetVisibleNodeCount(), 4u);
    manager.InjectMouseWheel(0.0f, -48.0f);
    EXPECT_FLOAT_EQ(tree->GetScrollY(), 48.0f);
    Click(70.0f, 12.0f);
    EXPECT_EQ(tree->GetSelectedNode(), grandchild);
    manager.Render();
    EXPECT_EQ(renderer->bucket.front().type, RenderCommandType::PushScissor);
    EXPECT_EQ(renderer->bucket.back().type, RenderCommandType::PopScissor);
    tree->SetExpanded(parent, false);
    EXPECT_FLOAT_EQ(tree->GetScrollY(), 0.0f);
    tree->ClearNodes();
    EXPECT_EQ(tree->GetVisibleNodeCount(), 0u);
    EXPECT_EQ(tree->GetSelectedNode(), UITreeView::RootNode);
    EXPECT_EQ(changes, 3);
}

TEST_F(CompoundWidgets, PopupRendersLastClampsAndConsumesOutsideDismissal) {
    manager.SetDisplaySize(200.0f, 100.0f);
    auto* popup = manager.GetOverlayRoot().CreateChild<UIButton>("Popup");
    popup->SetSize(80.0f, 50.0f);
    popup->SetNormalColor(0xFF112233u);
    auto* background = manager.GetRoot().CreateChild<UIButton>("Background");
    background->SetAllPoints(&manager.GetRoot());
    int backgroundClicks = 0;
    background->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++backgroundClicks; });
    manager.ShowPopup(*popup, 190.0f, 95.0f);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_FLOAT_EQ(popup->GetComputedRect().x, 120.0f);
    EXPECT_FLOAT_EQ(popup->GetComputedRect().y, 50.0f);
    EXPECT_EQ(renderer->bucket.back().color, 0xFF112233u);
    Click(10.0f, 10.0f);
    EXPECT_EQ(manager.GetActivePopup(), nullptr);
    EXPECT_EQ(backgroundClicks, 0);
    Click(10.0f, 10.0f);
    EXPECT_EQ(backgroundClicks, 1);
    manager.ShowPopup(*popup, 0.0f, 0.0f);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    EXPECT_EQ(manager.GetActivePopup(), nullptr);
}

TEST_F(CompoundWidgets, TooltipUsesInjectedTimeClampsAndDoesNotCaptureInput) {
    manager.SetDisplaySize(120.0f, 60.0f);
    manager.SetTooltipDelay(0.5f);
    auto* button = manager.GetRoot().CreateChild<UIButton>();
    button->SetAllPoints(&manager.GetRoot());
    button->SetTooltip("Details");
    int clicks = 0;
    button->RegisterCallback(UIEventType::OnClick, [&](const UIEventData&) { ++clicks; });
    manager.Update(0.0f);
    manager.InjectMouseMove(115.0f, 55.0f);
    manager.Update(0.49f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 1u);
    manager.Update(0.01f);
    manager.Render();
    ASSERT_EQ(renderer->bucket.size(), 5u);
    EXPECT_EQ(renderer->bucket[3].text, "Details");
    EXPECT_LE(renderer->bucket[1].x + renderer->bucket[1].width, 120.0f);
    EXPECT_LE(renderer->bucket[1].y + renderer->bucket[1].height, 60.0f);
    Click(115.0f, 55.0f);
    EXPECT_EQ(clicks, 1);
    manager.Render();
    EXPECT_EQ(renderer->bucket.size(), 1u);
    manager.Update(0.5f);
    button->SetVisible(false);
    manager.Update(0.0f);
    manager.Render();
    EXPECT_TRUE(renderer->bucket.empty());
}

TEST_F(CompoundWidgets, ContextMenuActionsDisabledRowsReplacementAndScrolling) {
    manager.SetDisplaySize(200.0f, 72.0f);
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager, "Context");
    int actions = 0;
    menu->SetItems({{"Disabled", [&] { actions += 100; }, false},
                    {"", {}, true, true}, {"Run", [&] { ++actions; }},
                    {"Last", [&] { actions += 10; }}});
    menu->Open(0.0f, 0.0f);
    Click(20.0f, 12.0f);
    EXPECT_TRUE(menu->IsOpen());
    EXPECT_EQ(actions, 0);
    Click(20.0f, 36.0f);
    EXPECT_TRUE(menu->IsOpen());
    manager.InjectMouseWheel(0.0f, -24.0f);
    EXPECT_FLOAT_EQ(menu->GetScrollY(), 24.0f);
    Click(20.0f, 60.0f);
    EXPECT_EQ(actions, 10);
    EXPECT_FALSE(menu->IsOpen());
    menu->SetItems({{"Replacement", [&] { ++actions; }}});
    menu->Open(0.0f, 0.0f);
    Click(20.0f, 12.0f, MouseButton::Right);
    EXPECT_TRUE(menu->IsOpen());
    Click(20.0f, 12.0f);
    EXPECT_EQ(actions, 11);
    menu->SetItems({});
    menu->Open(0.0f, 0.0f);
    EXPECT_FALSE(menu->IsOpen());
}

TEST_F(CompoundWidgets, MenuBarOpensSwitchesOnHoverTogglesAndClosesWithHiddenOwner) {
    auto* bar = manager.GetRoot().CreateChild<UIMenuBar>(manager, "MenuBar");
    bar->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    bar->SetSize(200.0f, 28.0f);
    int actions = 0;
    auto* first = bar->AddMenu("File", {{"Open", [&] { ++actions; }}});
    auto* second = bar->AddMenu("Edit", {{"Copy", [&] { actions += 10; }}});
    Click(20.0f, 10.0f);
    EXPECT_TRUE(first->IsOpen());
    manager.InjectMouseMove(150.0f, 10.0f);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_TRUE(second->IsOpen());
    Click(120.0f, 40.0f);
    EXPECT_EQ(actions, 10);
    Click(20.0f, 10.0f);
    EXPECT_TRUE(first->IsOpen());
    Click(20.0f, 10.0f);
    EXPECT_FALSE(first->IsOpen());
    Click(20.0f, 10.0f);
    bar->SetVisible(false);
    manager.Update(0.0f);
    EXPECT_FALSE(first->IsOpen());
    EXPECT_EQ(bar->GetMenuCount(), 2u);
    EXPECT_EQ(bar->GetMenu(2), nullptr);
}

TEST_F(CompoundWidgets, PopupResizesBackAfterViewportGrowthAndEscapesAncestorClipping) {
    manager.SetDisplaySize(400.0f, 200.0f);
    auto* scroll = manager.GetRoot().CreateChild<UIScrollContainer>();
    scroll->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    scroll->SetSize(40.0f, 40.0f);
    scroll->SetContentSize(40.0f, 40.0f);
    auto* owner = scroll->GetContent()->CreateChild<UIButton>();
    owner->SetAllPoints(scroll->GetContent());
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager);
    int actions = 0;
    menu->SetItems({{"Action", [&] { ++actions; }}});
    owner->RegisterCallback(UIEventType::OnClick, [&](const UIEventData& data) {
        if (data.button == MouseButton::Right) menu->Open(50.0f, 50.0f, owner);
    });
    Click(10.0f, 10.0f, MouseButton::Right);
    EXPECT_TRUE(menu->IsOpen());
    manager.Render();
    EXPECT_EQ(renderer->bucket[2].type, RenderCommandType::PopScissor);
    EXPECT_EQ(renderer->bucket[3].type, RenderCommandType::PushScissor);
    manager.SetDisplaySize(80.0f, 60.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(menu->GetComputedRect().width, 80.0f);
    manager.SetDisplaySize(400.0f, 200.0f);
    manager.Update(0.0f);
    EXPECT_FLOAT_EQ(menu->GetComputedRect().width, 160.0f);
    Click(60.0f, 60.0f);
    EXPECT_EQ(actions, 1);
}

TEST_F(CompoundWidgets, MenuActionMayReplaceItemsAndReopenSafely) {
    auto* menu = manager.GetOverlayRoot().CreateChild<UIContextMenu>(manager);
    int actions = 0;
    menu->SetItems({{"Replace", [&] {
        ++actions;
        menu->SetItems({{"New action", [&] { actions += 10; }}});
        menu->Open(0.0f, 0.0f);
    }}});
    menu->Open(0.0f, 0.0f);
    Click(10.0f, 10.0f);
    EXPECT_TRUE(menu->IsOpen());
    EXPECT_EQ(actions, 1);
    Click(10.0f, 10.0f);
    EXPECT_EQ(actions, 11);
    EXPECT_FALSE(menu->IsOpen());
}

TEST_F(CompoundWidgets, HiddenTabInputDoesNotReceiveCharactersAndPopupInputDoes) {
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>();
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->SetSize(200.0f, 100.0f);
    auto* first = tabs->AddTab("First");
    tabs->AddTab("Second");
    auto* input = first->CreateChild<UIInputBox>();
    input->SetAllPoints(first);
    Click(10.0f, 40.0f);
    manager.InjectCharacter(U'a');
    tabs->SetSelectedIndex(1);
    manager.InjectCharacter(U'b');
    EXPECT_EQ(input->GetText(), "a");
    auto* popup = manager.GetOverlayRoot().CreateChild<UIWidget>();
    popup->SetSize(100.0f, 40.0f);
    auto* popupInput = popup->CreateChild<UIInputBox>();
    popupInput->SetAllPoints(popup);
    manager.ShowPopup(*popup, 0.0f, 0.0f);
    Click(10.0f, 10.0f);
    manager.InjectCharacter(U'x');
    EXPECT_EQ(popupInput->GetText(), "x");
    manager.InjectKeyEvent(ScanCode::Backspace, true);
    EXPECT_EQ(popupInput->GetText(), "");
}

TEST_F(CompoundWidgets, DropdownChevronFlipsAndLabelCannotCoverIndicator) {
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>("IdentifiableDropdown");
    dropdown->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    dropdown->SetSize(160, 28);
    dropdown->SetOptions({"A long selected option that must not obscure the arrow", "Other"});
    manager.Update(0);
    manager.Render();
    std::vector<UIRenderCommand> arrow;
    bool labelClipped = false;
    for (const auto& command : renderer->bucket) {
        if (command.type == RenderCommandType::DrawQuad && command.color == 0xE5EBEFFFu) arrow.push_back(command);
        if (command.type == RenderCommandType::PushScissor) {
            EXPECT_LE(command.x + command.width, 132);
            labelClipped = true;
        }
        if (command.type == RenderCommandType::DrawString) EXPECT_TRUE(labelClipped);
        if (command.type == RenderCommandType::PopScissor) labelClipped = false;
    }
    ASSERT_EQ(arrow.size(), 6u);
    EXPECT_LT(arrow.front().y, arrow.back().y);
    EXPECT_GE(arrow.front().x, 132);
    Click(146, 14);
    ASSERT_TRUE(dropdown->IsExpanded());
    manager.Render();
    arrow.clear();
    bool focusedOutline = false;
    for (const auto& command : renderer->bucket) {
        if (command.type == RenderCommandType::DrawQuad && command.color == 0xE5EBEFFFu) arrow.push_back(command);
        focusedOutline |= command.type == RenderCommandType::DrawQuad && command.color == 0x67DBB3FFu;
    }
    ASSERT_EQ(arrow.size(), 6u);
    EXPECT_GT(arrow.front().y, arrow.back().y);
    EXPECT_TRUE(focusedOutline);
    Click(12, 64);
    EXPECT_EQ(dropdown->GetSelectedIndex(), 1);
    EXPECT_FALSE(dropdown->IsExpanded());
}

TEST_F(CompoundWidgets, TabsHaveSeparatedHeadersAndAccentTracksSelection) {
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>("IdentifiableTabs");
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->SetSize(200, 100);
    tabs->AddTab("First long tab label");
    tabs->AddTab("Second");
    manager.Update(0);
    for (int selected = 0; selected < 2; ++selected) {
        tabs->SetSelectedIndex(selected);
        manager.Render();
        int accents = 0;
        int inactiveEdges = 0;
        for (const auto& command : renderer->bucket) {
            if (command.type != RenderCommandType::DrawQuad) continue;
            if (command.color == 0x67DBB3FFu) {
                ++accents;
                EXPECT_FLOAT_EQ(command.x, selected * 100.0f + 1);
                EXPECT_FLOAT_EQ(command.y, 0);
                EXPECT_FLOAT_EQ(command.width, 98);
                EXPECT_FLOAT_EQ(command.height, 2);
            }
            if (command.color == 0x71808AFFu) {
                ++inactiveEdges;
                EXPECT_FLOAT_EQ(command.y, 26);
            }
        }
        EXPECT_EQ(accents, 1);
        EXPECT_EQ(inactiveEdges, 1);
        EXPECT_FLOAT_EQ(tabs->GetPage(selected)->GetComputedRect().y, 28);
    }
}

TEST_F(CompoundWidgets, DropdownAndTabDecorationsStayInsideTinyBounds) {
    auto* dropdown = manager.GetRoot().CreateChild<UIDropDownBox>();
    dropdown->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    dropdown->SetOptions({"Long option"});
    auto* tabs = manager.GetRoot().CreateChild<UITabControl>();
    tabs->SetPoint(AnchorPoint::TopLeft, &manager.GetRoot(), AnchorPoint::TopLeft);
    tabs->AddTab("Long tab");
    tabs->AddTab("Second");
    for (float size : {0.0f, 0.5f, 2.0f, 8.0f}) {
        dropdown->SetSize(size, size);
        tabs->SetSize(size, size);
        manager.Update(0);
        manager.Render();
        for (const auto& command : renderer->bucket) {
            if (command.type != RenderCommandType::DrawQuad && command.type != RenderCommandType::PushScissor) continue;
            EXPECT_GE(command.x, 0);
            EXPECT_GE(command.y, 0);
            EXPECT_GE(command.width, 0);
            EXPECT_GE(command.height, 0);
            EXPECT_LE(command.x + command.width, size);
            EXPECT_LE(command.y + command.height, size);
        }
    }
}