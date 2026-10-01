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

class LuaBindingsTest : public testing::Test {
protected:
    std::unique_ptr<lua_State, decltype(&lua_close)> lua{luaL_newstate(), lua_close};
    UIManager manager{nullptr};
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