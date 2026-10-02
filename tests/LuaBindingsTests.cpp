#include "LambUI/LambUI.h"
#include "LambUILua/LuaBindings.h"
#include <gtest/gtest.h>
#include <stdexcept>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

using namespace LambUI;

namespace {
constexpr const char* TAG = "LuaBindingsTest";

class LuaCanvasRenderer : public IRenderer {
public:
    std::vector<UIRenderCommand> commands;
    LuaCanvasRenderer() { LAMBUI_LOGT(TAG, "Construct Lua canvas test renderer"); }
    ~LuaCanvasRenderer() override { LAMBUI_LOGT(TAG, "Destroy Lua canvas test renderer"); }
    void SubmitRenderCommands(const std::vector<UIRenderCommand>& bucket) override {
        LAMBUI_LOGT(TAG, "Record Lua canvas commands");
        commands = bucket;
        for (const auto& command : commands)
            if (command.customRenderFunc) command.customRenderFunc({command.x, command.y, command.width, command.height, command.customRenderUserData});
    }
};

class LuaBindingsTest : public testing::Test {
protected:
    std::unique_ptr<lua_State, decltype(&lua_close)> lua{luaL_newstate(), lua_close};
    std::shared_ptr<LuaCanvasRenderer> renderer = std::make_shared<LuaCanvasRenderer>();
    UIManager manager{renderer};
    std::unique_ptr<LambUILua::LuaUIBindings> bindings;

    LuaBindingsTest() { LAMBUI_LOGT(TAG, "Construct"); }
    ~LuaBindingsTest() override { LAMBUI_LOGT(TAG, "Destroy"); }

    void SetUp() override {
        LAMBUI_LOGT(TAG, "SetUp");
        ASSERT_NE(lua, nullptr);
        luaL_openlibs(lua.get());
        bindings = std::make_unique<LambUILua::LuaUIBindings>(lua.get(), manager);
        manager.SetDisplaySize(400, 300);
    }

    testing::AssertionResult Run(const char* script) {
        LAMBUI_LOGT(TAG, "Run script");
        const int top = lua_gettop(lua.get());
        const int status = luaL_dostring(lua.get(), script);
        std::string error;
        if (status != LUA_OK) {
            const char* message = lua_tostring(lua.get(), -1);
            error = message ? message : "non-string error";
        }
        lua_settop(lua.get(), top);
        return status == LUA_OK ? testing::AssertionSuccess() : testing::AssertionFailure() << error;
    }
};
}

TEST_F(LuaBindingsTest, ExistingScriptingSurfaceAndOptionalFactoryArguments) {
    ASSERT_TRUE(Run(R"lua(
        frame = UI.CreateFrame("Frame", "Panel")
        frame:SetSize(200, 100)
        frame:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 20, 30)
        assert(frame:GetName() == "Panel" and frame:IsVisible())
        frame:SetVisible(false)
        assert(not frame:IsVisible())
        frame:SetVisible(true)
        assert(UI.CreateFrame("Frame"):GetName() == "")
        assert(UI.CreateFrame("Frame", nil, nil):GetName() == "")
        bar = frame:CreateStatusBar("Health")
        bar:SetMinMaxValues(0, 100)
        bar:SetValue(150)
        assert(bar:GetValue() == 100)
        texture = frame:CreateTexture()
        texture:SetAllPoints(frame)
        texture:SetTexture(123)
        texture:SetTint(0xAABBCCFF)
        text = frame:CreateFontString("Label")
        text:SetText("Hello\0Lua")
        text:SetColor(0xFFFFFFFF)
        UI.CreateFrame("StatusBar", "Other"):SetValue(0.5)
        UI.CreateFrame("EditBox", "Input"):SetSize(80, 20)
        button = UI.CreateFrame("Button", "Button", frame)
        button:SetAllPoints(frame)
    )lua"));
    manager.Update(0);
    manager.InjectMouseMove(25, 35);
    manager.InjectMouseButton(MouseButton::Left, true);
    auto* button = manager.GetFocusedWidget();
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->GetName(), "Button");
    EXPECT_FLOAT_EQ(button->GetComputedRect().x, 20);
    EXPECT_FLOAT_EQ(button->GetComputedRect().width, 200);
}

TEST_F(LuaBindingsTest, DropdownSelectionUsesOneBasedIndicesAndValidatesArguments) {
    ASSERT_TRUE(Run(R"lua(
        dropdown = UI.CreateFrame("DropDownBox", "Choice")
        assert(dropdown:GetSelectedIndex() == 0)
        dropdown:SetOptions({"First", "Second"})
        assert(dropdown:GetSelectedIndex() == 1)
        changes = 0
        dropdown:SetScript("OnValueChanged", function() changes = changes + 1 end)
        dropdown:SetSelectedIndex(2)
        assert(dropdown:GetSelectedIndex() == 2 and changes == 1)
        dropdown:SetSelectedIndex(3)
        assert(dropdown:GetSelectedIndex() == 2 and changes == 1)
        assert(not pcall(function() dropdown:SetSelectedIndex(0) end))
        assert(not pcall(function() dropdown:SetSelectedIndex(1.5) end))
        assert(not pcall(function() dropdown:SetSelectedIndex(0x7FFFFFFFFFFFFFFF) end))
        assert(not pcall(function() dropdown:SetOptions("invalid") end))
        assert(not pcall(function() dropdown:SetOptions({"First", false}) end))
        assert(not pcall(function() UI.Root:SetOptions({"First"}) end))
        assert(dropdown:GetSelectedIndex() == 2)
        dropdown:SetOptions({})
        assert(dropdown:GetSelectedIndex() == 0)
    )lua"));
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, DropdownFontsApplyToSelectionAndPopupOptions) {
    bindings->SetFontResolver([](const std::string& name, int size) -> void* {
        return name == "Body" && size == 20 ? reinterpret_cast<void*>(uintptr_t{123}) : nullptr;
    });
    ASSERT_TRUE(Run(R"lua(
        dropdown = UI.CreateFrame("DropDownBox", "Choice")
        dropdown:SetSize(160, 28)
        dropdown:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 20, 20)
        dropdown:SetFont("Body", 20)
        dropdown:SetOptions({"First", "Second"})
    )lua"));
    const auto expectFonts = [&](uintptr_t handle, int expectedCount) {
        manager.Update(0);
        manager.Render();
        int count = 0;
        for (const auto& command : renderer->commands) {
            if (command.type == RenderCommandType::DrawString && !command.text.empty()) {
                ++count;
                EXPECT_EQ(command.fontHandle, reinterpret_cast<void*>(handle));
            }
        }
        EXPECT_EQ(count, expectedCount);
    };
    const auto openPopup = [&]() {
        manager.InjectMouseMove(30, 30);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseButton(MouseButton::Left, false);
    };
    expectFonts(123, 1);
    openPopup();
    expectFonts(123, 3);
    ASSERT_TRUE(Run("dropdown:SetFont(456)"));
    expectFonts(456, 3);
    ASSERT_TRUE(Run(R"lua(
        assert(not pcall(function() dropdown:SetFont("Missing", 20) end))
        assert(not pcall(function() dropdown:SetFont("Body", 0) end))
        dropdown:SetOptions({"Third", "Fourth", "Fifth"})
    )lua"));
    expectFonts(456, 1);
    openPopup();
    expectFonts(456, 4);
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, DropdownPopupEscapesParentClipScrollsAndDismisses) {
    ASSERT_TRUE(Run(R"lua(
        page = UI.CreateFrame("ScrollContainer", "Page")
        page:SetSize(200, 40)
        page:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 20, 220)
        page:SetContentSize(200, 40)
        dropdown = UI.CreateFrame("DropDownBox", "Choice", page:GetContent())
        dropdown:SetSize(160, 28)
        dropdown:SetPoint("TOPLEFT", page:GetContent(), "TOPLEFT", 0, 0)
        options = {}
        for index = 1, 30 do options[index] = "Option " .. index end
        dropdown:SetOptions(options)
    )lua"));
    const auto click = [&](float horizontal, float vertical) {
        manager.InjectMouseMove(horizontal, vertical);
        manager.InjectMouseButton(MouseButton::Left, true);
        manager.InjectMouseButton(MouseButton::Left, false);
        manager.Update(0);
    };
    manager.Update(0);
    click(40, 230);
    auto* popup = dynamic_cast<UIContextMenu*>(manager.GetActivePopup());
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->IsOpen());
    EXPECT_FLOAT_EQ(popup->GetComputedRect().width, 160);
    EXPECT_FLOAT_EQ(popup->GetComputedRect().y, 0);
    EXPECT_FLOAT_EQ(popup->GetComputedRect().height, 220);
    click(40, 36);
    ASSERT_TRUE(Run("assert(dropdown:GetSelectedIndex() == 2)"));
    EXPECT_FALSE(popup->IsOpen());
    click(40, 230);
    manager.InjectMouseMove(40, 100);
    manager.InjectMouseWheel(0, -1000);
    manager.Update(0);
    click(40, 208);
    ASSERT_TRUE(Run("assert(dropdown:GetSelectedIndex() == 30)"));
    click(40, 230);
    manager.InjectKeyEvent(ScanCode::Escape, true);
    manager.InjectKeyEvent(ScanCode::Escape, false);
    EXPECT_FALSE(popup->IsOpen());
    click(40, 230);
    click(350, 280);
    EXPECT_FALSE(popup->IsOpen());
    ASSERT_TRUE(Run("assert(dropdown:GetSelectedIndex() == 30)"));
}

TEST_F(LuaBindingsTest, NamedFontsResolveFaceAndSizeAndPreserveNumericHandles) {
    ASSERT_TRUE(Run(R"lua(
        label = UI.Root:CreateFontString("Title")
        label:SetText("Title")
        assert(not pcall(function() label:SetFont("Heading", 32) end))
    )lua"));
    bindings->SetFontResolver([](const std::string& name, int size) -> void* {
        if (name == "Broken") throw std::runtime_error("Font resolver failed");
        if (name == "Heading" && size == 32) return reinterpret_cast<void*>(uintptr_t{123});
        if (name == "Body" && size == 0) return reinterpret_cast<void*>(uintptr_t{456});
        return nullptr;
    });
    ASSERT_TRUE(Run(R"lua(
        label:SetFont("Heading", 32)
        input = UI.CreateFrame("EditBox")
        input:SetFont("Body")
        input:SetText("Body")
        input:SetSize(100, 30)
        assert(not pcall(function() label:SetFont("Missing", 32) end))
        assert(not pcall(function() label:SetFont("Heading", 99) end))
        assert(not pcall(function() label:SetFont("Heading", 0) end))
        assert(not pcall(function() label:SetFont("Heading", -1) end))
        assert(not pcall(function() label:SetFont("Heading", 1.5) end))
        assert(not pcall(function() label:SetFont("Broken", 32) end))
        assert(not pcall(function() UI.Root:SetFont("Body") end))
    )lua"));
    manager.Update(0);
    manager.Render();
    bool titleFound = false;
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::DrawString && command.text == "Title") {
            titleFound = true;
            EXPECT_EQ(command.fontHandle, reinterpret_cast<void*>(uintptr_t{123}));
        }
    }
    EXPECT_TRUE(titleFound);
    ASSERT_TRUE(Run("label:SetFont(456)"));
    manager.Update(0);
    manager.Render();
    for (const auto& command : renderer->commands) {
        if (command.type == RenderCommandType::DrawString && command.text == "Title")
            EXPECT_EQ(command.fontHandle, reinterpret_cast<void*>(uintptr_t{456}));
    }
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, ImageWidgetsUseHostLoaderAndValidateFit) {
    int texture = 0;
    int loads = 0;
    bindings->SetImageLoader([&](const std::string& source) {
        ++loads;
        return source == "valid.png" ? UIImage{&texture, 200, 100} : UIImage{};
    });
    ASSERT_TRUE(Run(R"lua(
        image = UI.Root:CreateImage("Artwork")
        image:SetAllPoints(UI.Root)
        assert(image:GetName() == "Artwork")
        assert(image:GetFit() == "CONTAIN")
        assert(not image:IsLoaded())
        assert(image:SetSource("valid.png"))
        assert(image:IsLoaded() and image:GetSource() == "valid.png")
        image:SetFit("COVER")
        assert(image:GetFit() == "COVER")
        image:SetFit("STRETCH")
        assert(image:GetFit() == "STRETCH")
        image:SetTint(0xAABBCCFF)
        assert(not pcall(function() image:SetFit("INVALID") end))
        assert(not pcall(function() image:SetSource(42) end))
        assert(not pcall(function() UI.Root:SetSource("valid.png") end))
        assert(not image:SetSource("missing.png"))
        assert(not image:IsLoaded())
        assert(not image:SetSource(""))
        assert(UI.CreateFrame("Image"):SetSource("valid.png"))
        assert(UI.Root:CreateImage():GetName() == "")
    )lua"));
    EXPECT_EQ(loads, 3);
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, ImageButtonSwapsPreloadedImagesInCallbacks) {
    int normalTexture = 0;
    int hoverTexture = 0;
    int loads = 0;
    bindings->SetImageLoader([&](const std::string& source) {
        ++loads;
        return UIImage{source == "normal.png" ? &normalTexture : &hoverTexture, 260, 110};
    });
    ASSERT_TRUE(Run(R"lua(
        clicks = 0
        enters, leaves = 0, 0
        normalImage = assert(UI.LoadImage("normal.png"))
        hoverImage = assert(UI.LoadImage("hover.png"))
        button = UI.CreateFrame("Button", "ImageButton")
        button:SetSize(260, 110)
        button:SetPoint("CENTER", UI.Root, "CENTER", 0, 0)
        button:SetButtonColors(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
        assert(button:SetImage(normalImage))
        button:SetScript("OnEnter", function()
            assert(button:SetImage(hoverImage))
            enters = enters + 1
        end)
        button:SetScript("OnLeave", function()
            assert(button:SetImage(normalImage))
            leaves = leaves + 1
        end)
        button:SetScript("OnClick", function() clicks = clicks + 1 end)
    )lua"));
    const auto expectTexture = [&](void* texture) {
        manager.Render();
        int texturedQuads = 0;
        for (const auto& command : renderer->commands) {
            if (command.type != RenderCommandType::DrawQuad || !command.textureHandle) continue;
            ++texturedQuads;
            EXPECT_EQ(command.textureHandle, texture);
            EXPECT_EQ(command.color, 0xFFFFFFFFu);
        }
        EXPECT_EQ(texturedQuads, 1);
    };
    manager.Update(0);
    expectTexture(&normalTexture);
    manager.InjectMouseMove(200, 150);
    ASSERT_TRUE(Run("assert(enters == 1)"));
    manager.Update(0);
    expectTexture(&hoverTexture);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    ASSERT_TRUE(Run("assert(clicks == 1)"));
    manager.InjectMouseMove(0, 0);
    ASSERT_TRUE(Run("assert(leaves == 1)"));
    ASSERT_TRUE(Run("normalImage, hoverImage = nil, nil; collectgarbage('collect')"));
    manager.Update(0);
    expectTexture(&normalTexture);
    EXPECT_EQ(loads, 2);
}

TEST_F(LuaBindingsTest, PreloadedImagesValidateLoadsTypesAndBindingLifetime) {
    ASSERT_TRUE(Run("assert(UI.LoadImage('missing.png') == nil)"));
    int texture = 0;
    bindings->SetImageLoader([&](const std::string& source) {
        if (source == "valid.png") return UIImage{&texture, 260, 110};
        if (source == "invalid.png") return UIImage{&texture, 0, 110};
        if (source == "throws.png") throw std::runtime_error("load failed");
        return UIImage{};
    });
    ASSERT_TRUE(Run(R"lua(
        assert(UI.LoadImage("") == nil)
        assert(UI.LoadImage("missing.png") == nil)
        assert(UI.LoadImage("invalid.png") == nil)
        assert(not pcall(function() UI.LoadImage(42) end))
        assert(not pcall(function() UI.LoadImage("throws.png") end))
        image = assert(UI.LoadImage("valid.png"))
        button = UI.CreateFrame("Button")
        assert(not pcall(function() button:SetImage(nil) end))
        assert(not pcall(function() button:SetImage(123) end))
        assert(not pcall(function() button:SetImage({}) end))
        assert(not pcall(function() button:SetImage(button) end))
        assert(not pcall(function() UI.Root:SetImage(image) end))
        assert(button:SetImage(image))
        assert(UI.CreateFrame("Button"):SetImage(image))
    )lua"));
    manager.Clear();
    bindings.reset();
    bindings = std::make_unique<LambUILua::LuaUIBindings>(lua.get(), manager);
    ASSERT_TRUE(Run(R"lua(
        button = UI.CreateFrame("Button")
        assert(not pcall(function() button:SetImage(image) end))
        image = nil
        collectgarbage("collect")
    )lua"));
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, InjectedScriptsAndGameEventsPreserveStack) {
    ASSERT_TRUE(Run(R"lua(
        clicks, enters, leaves = 0, 0, 0
        button = UI.CreateFrame("Button", "Action")
        button:SetSize(100, 40)
        button:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 0, 0)
        button:SetScript("OnClick", function() clicks = clicks + 1 end)
        button:SetScript("OnEnter", function() enters = enters + 1 end)
        button:SetScript("OnLeave", function() leaves = leaves + 1 end)
        button:RegisterEvent("HEALTH", function(value) health = value end)
    )lua"));
    lua_pushliteral(lua.get(), "stack sentinel");
    const int top = lua_gettop(lua.get());
    manager.Update(0);
    manager.InjectMouseMove(20, 20);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectMouseMove(300, 200);
    manager.FireGameEvent("HEALTH", reinterpret_cast<void*>(uintptr_t{73}));
    EXPECT_EQ(lua_gettop(lua.get()), top);
    EXPECT_STREQ(lua_tostring(lua.get(), -1), "stack sentinel");
    ASSERT_TRUE(Run("assert(clicks == 1 and enters == 1 and leaves == 1 and health == 73)"));
    manager.FireGameEvent("HEALTH");
    ASSERT_TRUE(Run("assert(health == 0)"));
}

TEST_F(LuaBindingsTest, InvalidCallsRaiseLuaErrorsAndLeaveBindingUsable) {
    ASSERT_TRUE(Run(R"lua(
        frame = UI.CreateFrame("Frame")
        local invalid = {
            function() UI.CreateFrame("Unknown") end,
            function() UI.CreateFrame("Frame", {}, UI.Root) end,
            function() UI.CreateFrame("Frame", "Bad", {}) end,
            function() frame:SetSize("wide", 10) end,
            function() frame:SetSize(0/0, 10) end,
            function() frame:SetPoint("BAD", UI.Root, "TOPLEFT", 0, 0) end,
            function() frame:SetVisible(1) end,
            function() frame:SetValue(10) end,
            function() frame:RegisterEvent("EVENT", false) end,
            function() frame:CreateTexture():SetTint(-1) end,
            function() UI.CreateFrame("Button"):SetScript("Bad", function() end) end
        }
        for _, action in ipairs(invalid) do
            local ok, message = pcall(action)
            assert(not ok and string.find(message, "LambUI:"))
        end
        frame:SetSize(10, 20)
        assert(frame:IsVisible())
    )lua"));
    EXPECT_EQ(lua_gettop(lua.get()), 0);
    EXPECT_THROW(LambUILua::LuaUIBindings(nullptr, manager), std::invalid_argument);
    EXPECT_THROW(LambUILua::LuaUIBindings(lua.get(), manager), std::invalid_argument);
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, CallbackErrorsAreContainedAndLaterCallbacksStillRun) {
    ASSERT_TRUE(Run(R"lua(
        count = 0
        UI.Root:RegisterEvent("BROKEN", function() error({ reason = "failure" }) end)
        UI.Root:RegisterEvent("GOOD", function() count = count + 1 end)
    )lua"));
    for (int index = 0; index < 5; ++index) {
        manager.FireGameEvent("BROKEN");
        manager.FireGameEvent("GOOD");
        EXPECT_EQ(lua_gettop(lua.get()), 0);
    }
    ASSERT_TRUE(Run("assert(count == 5)"));
}

TEST_F(LuaBindingsTest, ReplacedAndClearedScriptsReleaseRegistryReferences) {
    ASSERT_TRUE(Run(R"lua(
        button = UI.CreateFrame("Button")
        weak = setmetatable({}, { __mode = "v" })
        do
            local handler = function() end
            weak[1] = handler
            button:SetScript("OnClick", handler)
        end
        collectgarbage("collect")
        assert(weak[1] ~= nil)
        button:SetScript("OnClick", function() end)
        collectgarbage("collect")
        assert(weak[1] == nil)
        do
            local handler = function() end
            weak[2] = handler
            button:SetScript("OnClick", handler)
        end
        button:SetScript("OnClick", nil)
        collectgarbage("collect")
        assert(weak[2] == nil)
    )lua"));
}

TEST_F(LuaBindingsTest, DetachmentInvalidatesHandlesAndReleasesCallbacks) {
    ASSERT_TRUE(Run(R"lua(
        oldRoot = UI.Root
        oldFactory = UI.CreateFrame
        count = 0
        weak = setmetatable({}, { __mode = "v" })
        do
            local handler = function() count = count + 1 end
            weak[1] = handler
            UI.Root:RegisterEvent("EVENT", handler)
        end
    )lua"));
    bindings.reset();
    manager.FireGameEvent("EVENT");
    ASSERT_TRUE(Run(R"lua(
        assert(count == 0)
        assert(not pcall(function() oldRoot:GetName() end))
        assert(not pcall(function() oldFactory("Frame") end))
        collectgarbage("collect")
        assert(weak[1] == nil)
    )lua"));
    bindings = std::make_unique<LambUILua::LuaUIBindings>(lua.get(), manager);
    ASSERT_TRUE(Run(R"lua(
        assert(not pcall(function() UI.CreateFrame("Frame", "Stale", oldRoot) end))
        assert(UI.CreateFrame("Frame", "New"):GetName() == "New")
    )lua"));
}

TEST_F(LuaBindingsTest, CoroutineRegistrationUsesMainStateRegistry) {
    ASSERT_TRUE(Run(R"lua(
        count = 0
        local thread = coroutine.create(function()
            local widget = UI.CreateFrame("Frame", "Coroutine")
            widget:RegisterEvent("EVENT", function() count = count + 1 end)
        end)
        assert(coroutine.resume(thread))
        thread = nil
        collectgarbage("collect")
    )lua"));
    manager.FireGameEvent("EVENT");
    ASSERT_TRUE(Run("assert(count == 1)"));
    EXPECT_EQ(lua_gettop(lua.get()), 0);
}

TEST_F(LuaBindingsTest, LuaCanCloseAfterDetachmentWhileManagerRemainsAlive) {
    ASSERT_TRUE(Run(R"lua(
        UI.Root:RegisterEvent("EVENT", function() error("must not run") end)
        button = UI.CreateFrame("Button")
        button:SetSize(100, 40)
        button:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 0, 0)
        button:SetScript("OnClick", function() error("must not run") end)
    )lua"));
    bindings.reset();
    lua.reset();
    manager.FireGameEvent("EVENT");
    manager.Update(0);
    manager.InjectMouseMove(10, 10);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
}

TEST_F(LuaBindingsTest, ApplicationControlsAndInjectedValueTextCallbacks) {
    ASSERT_TRUE(Run(R"lua(
        changes, edits = 0, 0
        check = UI.CreateFrame("CheckBox", "Ready")
        check:SetSize(150, 30)
        check:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 10, 10)
        check:SetText("Ready")
        check:SetTooltip("Ready for departure")
        check:SetScript("OnValueChanged", function() changes = changes + 1 end)
        input = UI.CreateFrame("EditBox", "Notes")
        input:SetSize(180, 80)
        input:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 10, 60)
        input:SetMultiline(true)
        input:SetWordWrap(true)
        input:SetFont(0)
        input:SetText("Notes")
        input:SetScript("OnTextChanged", function() edits = edits + 1 end)
        progress = UI.CreateFrame("ProgressBar", "Launch")
        progress:SetMinMaxValues(0, 100)
        progress:SetValue(200)
        progress:SetProgressColors(0x101010FF, 0x40C080FF)
        assert(progress:GetValue() == 100)
        assert(check:GetText() == "Ready" and input:GetText() == "Notes")
    )lua"));
    manager.Update(0);
    manager.InjectMouseMove(20, 20);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    ASSERT_TRUE(Run("assert(check:IsChecked() and changes == 1)"));
    manager.InjectMouseMove(20, 70);
    manager.InjectMouseButton(MouseButton::Left, true);
    manager.InjectMouseButton(MouseButton::Left, false);
    manager.InjectCharacter(U'X');
    ASSERT_TRUE(Run("assert(edits == 1 and #input:GetText() == 6); input:SetEditingEnabled(false)"));
    manager.InjectCharacter(U'Y');
    ASSERT_TRUE(Run("assert(edits == 1); input:SetScript('OnTextChanged', nil); input:SetText('Reset'); assert(edits == 1)"));
}

TEST_F(LuaBindingsTest, ApplicationScrollLayoutAndStrictMethodValidation) {
    ASSERT_TRUE(Run(R"lua(
        scroll = UI.CreateFrame("ScrollContainer", "Routes")
        scroll:SetSize(200, 100)
        scroll:SetPoint("TOPLEFT", UI.Root, "TOPLEFT", 20, 30)
        scroll:SetContentSize(200, 400)
        scroll:SetBackgroundColor(0x202020FF)
        child = UI.CreateFrame("Button", "Route", scroll:GetContent())
        child:SetSize(160, 40)
        child:SetPoint("TOPLEFT", scroll:GetContent(), "TOPLEFT", 0, 200)
        child:SetButtonColors(0x202020FF, 0x303030FF, 0x404040FF)
        label = child:CreateFontString("Name")
        label:SetText("Route")
        label:SetFont(0)
        label:SetMouseEnabled(false)
        label:SetWordWrap(true)
        label:SetSize(140, 30)
        label:SetPoint("TOPLEFT", child, "TOPLEFT", 4, 4)
        scroll:SetScrollOffset(0, 180)
        local invalid = {
            function() child:SetChecked(true) end,
            function() scroll:SetMultiline(true) end,
            function() child:SetMouseEnabled(1) end,
            function() child:SetButtonColors(0, -1, 0) end,
            function() scroll:SetContentSize(0/0, 100) end,
            function() UI.Root:GetContent() end
        }
        for _, action in ipairs(invalid) do assert(not pcall(action)) end
    )lua"));
    manager.Update(0);
    ASSERT_TRUE(Run("local x,y,w,h = child:GetRect(); assert(x == 20 and y == 50 and w == 160 and h == 40)"));
    ASSERT_TRUE(Run("child:ClearPoints(); child:SetPoint('TOPLEFT', scroll:GetContent(), 'TOPLEFT', 10, 210)"));
    manager.Update(0);
    ASSERT_TRUE(Run("local x,y = child:GetRect(); assert(x == 30 and y == 60)"));
}

TEST_F(LuaBindingsTest, WindowsAndCanvasUseCommandCallbacksWithSafeDetachment) {
    auto renderer = std::make_shared<LuaCanvasRenderer>();
    UIManager canvasManager(renderer);
    canvasManager.SetDisplaySize(400, 300);
    bindings.reset();
    bindings = std::make_unique<LambUILua::LuaUIBindings>(lua.get(), canvasManager);
    ASSERT_TRUE(Run(R"lua(
        draws, states, closes = 0, 0, 0
        window = UI.CreateFrame("Window", "Preview")
        window:SetTitle("Canvas preview")
        window:SetSizeLimits(180, 120, 500, 400)
        window:SetBounds(20, 30, 250, 200)
        window:SetMovable(true)
        window:SetResizable(true)
        window:SetScript("OnWindowStateChanged", function() states = states + 1 end)
        window:SetScript("OnClose", function() closes = closes + 1 end)
        canvas = UI.CreateFrame("Canvas", "Field", window:GetContent())
        canvas:SetAllPoints(window:GetContent())
        canvas:SetRenderCallback(function(x,y,width,height)
            draws = draws + 1
            assert(x == 26 and y == 62 and width == 238 and height == 162)
        end)
        assert(not pcall(function() window:SetRenderCallback(function() end) end))
        assert(not pcall(function() canvas:SetBounds(0,0,100,100) end))
        assert(not pcall(function() canvas:SetRenderCallback(42) end))
    )lua"));
    canvasManager.Update(0);
    lua_pushliteral(lua.get(), "sentinel");
    const int top = lua_gettop(lua.get());
    canvasManager.Render();
    EXPECT_EQ(lua_gettop(lua.get()), top);
    ASSERT_TRUE(Run("assert(draws == 1); window:Minimize(); assert(window:GetWindowState() == 'Minimized')"));
    canvasManager.Update(0); canvasManager.Render();
    ASSERT_TRUE(Run("assert(draws == 1); window:Restore(); window:BringToFront(); assert(states == 2)"));
    canvasManager.Update(0); canvasManager.Render();
    ASSERT_TRUE(Run("assert(draws == 2); window:Maximize(); assert(window:GetWindowState() == 'Maximized'); window:Restore()"));
    ASSERT_TRUE(Run("window:Close(); assert(closes == 1 and not window:IsVisible()); window:SetVisible(true)"));
    ASSERT_TRUE(Run(R"lua(
        weak = setmetatable({}, {__mode = 'v'})
        do local callback = function() draws = draws + 10 end
            weak[1] = callback; canvas:SetRenderCallback(callback)
        end
        canvas:SetRenderCallback(nil); collectgarbage('collect'); assert(weak[1] == nil)
        canvas:SetRenderCallback(function() error('contained canvas error') end)
    )lua"));
    canvasManager.Update(0); canvasManager.Render();
    EXPECT_EQ(lua_gettop(lua.get()), top);
    ASSERT_TRUE(Run("canvas:SetRenderCallback(function() draws = draws + 1 end)"));
    canvasManager.Render();
    ASSERT_TRUE(Run("assert(draws == 3)"));
    bindings.reset();
    canvasManager.Render();
    ASSERT_TRUE(Run("assert(draws == 3)"));
}