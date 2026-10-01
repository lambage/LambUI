#include "LambUILua/LuaBindings.h"
#include "LambUI/LambUI.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_set>

using namespace LambUI;

namespace {
constexpr const char* TAG = "LuaUIBindings";
constexpr const char* WidgetMetatable = "LambUI.Widget";
char BindingRegistryKey;
}

namespace LambUILua { namespace Detail {

struct BindingState {
    lua_State* lua;
    UIManager& manager;
    bool active = true;
    std::unordered_set<int> references;

    BindingState(lua_State* state, UIManager& owner) : lua(state), manager(owner) {
        LAMBUI_LOGT(TAG, "Construct binding state");
    }

    void Detach() {
        if (!active) return;
        LAMBUI_LOGT(TAG, "Detach {} Lua callbacks", references.size());
        active = false;
        for (int reference : references) luaL_unref(lua, LUA_REGISTRYINDEX, reference);
        references.clear();
    }

    ~BindingState() {
        LAMBUI_LOGT(TAG, "Destroy binding state");
        Detach();
    }
};

} }

namespace {
using BindingState = LambUILua::Detail::BindingState;

struct WidgetHandle {
    UIWidget* widget;
    std::weak_ptr<BindingState> state;
};

struct LuaCallback {
    std::weak_ptr<BindingState> state;
    int reference;

    LuaCallback(const std::shared_ptr<BindingState>& owner, lua_State* source, int index)
        : state(owner) {
        LAMBUI_LOGT(TAG, "Retain Lua callback");
        lua_pushvalue(source, index);
        reference = luaL_ref(source, LUA_REGISTRYINDEX);
        owner->references.insert(reference);
    }

    ~LuaCallback() {
        LAMBUI_LOGT(TAG, "Release Lua callback");
        if (auto owner = state.lock()) {
            if (owner->active) {
                owner->references.erase(reference);
                luaL_unref(owner->lua, LUA_REGISTRYINDEX, reference);
            }
        }
    }

    void Invoke(bool hasPayload, void* payload = nullptr) const {
        auto owner = state.lock();
        if (!owner || !owner->active) return;
        LAMBUI_LOGT(TAG, "Invoke Lua callback");
        lua_State* lua = owner->lua;
        const int top = lua_gettop(lua);
        lua_rawgeti(lua, LUA_REGISTRYINDEX, reference);
        if (hasPayload) lua_pushinteger(lua, static_cast<lua_Integer>(reinterpret_cast<uintptr_t>(payload)));
        if (lua_pcall(lua, hasPayload ? 1 : 0, 0, 0) != LUA_OK) {
            const char* error = lua_tostring(lua, -1);
            LAMBUI_LOGE(TAG, "Lua callback failed: {}", error ? error : "non-string error");
        }
        lua_settop(lua, top);
    }
};

enum class Operation {
    CreateFrame, SetSize, SetPoint, SetAllPoints, SetVisible, IsVisible, GetName,
    RegisterEvent, CreateTexture, CreateStatusBar, CreateFontString, SetScript,
    SetTexture, SetTint, SetMinMaxValues, SetValue, GetValue, SetText, SetColor
};

const char* ToString(Operation operation) {
    switch (operation) {
        case Operation::CreateFrame: return "CreateFrame";
        case Operation::SetSize: return "SetSize";
        case Operation::SetPoint: return "SetPoint";
        case Operation::SetAllPoints: return "SetAllPoints";
        case Operation::SetVisible: return "SetVisible";
        case Operation::IsVisible: return "IsVisible";
        case Operation::GetName: return "GetName";
        case Operation::RegisterEvent: return "RegisterEvent";
        case Operation::CreateTexture: return "CreateTexture";
        case Operation::CreateStatusBar: return "CreateStatusBar";
        case Operation::CreateFontString: return "CreateFontString";
        case Operation::SetScript: return "SetScript";
        case Operation::SetTexture: return "SetTexture";
        case Operation::SetTint: return "SetTint";
        case Operation::SetMinMaxValues: return "SetMinMaxValues";
        case Operation::SetValue: return "SetValue";
        case Operation::GetValue: return "GetValue";
        case Operation::SetText: return "SetText";
        case Operation::SetColor: return "SetColor";
    }
    return "Unknown";
}

std::string String(lua_State* lua, int index) {
    if (lua_type(lua, index) != LUA_TSTRING) throw std::invalid_argument("LambUI: expected a string");
    size_t length = 0;
    const char* value = lua_tolstring(lua, index, &length);
    return std::string(value, length);
}

float Number(lua_State* lua, int index) {
    if (lua_type(lua, index) != LUA_TNUMBER) throw std::invalid_argument("LambUI: expected a number");
    const auto value = lua_tonumber(lua, index);
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        throw std::invalid_argument("LambUI: expected a finite float");
    return static_cast<float>(value);
}

lua_Integer Integer(lua_State* lua, int index) {
    if (!lua_isinteger(lua, index)) throw std::invalid_argument("LambUI: expected an integer");
    return lua_tointeger(lua, index);
}

uint32_t Color(lua_State* lua, int index) {
    const auto value = Integer(lua, index);
    if (value < 0 || static_cast<lua_Unsigned>(value) > UINT32_MAX)
        throw std::invalid_argument("LambUI: expected a 32-bit RGBA color");
    return static_cast<uint32_t>(value);
}

WidgetHandle& Handle(lua_State* lua, int index) {
    auto* handle = static_cast<WidgetHandle*>(luaL_testudata(lua, index, WidgetMetatable));
    if (!handle) throw std::invalid_argument("LambUI: expected a widget");
    return *handle;
}

UIWidget& Widget(lua_State* lua, int index, const BindingState& owner) {
    auto& handle = Handle(lua, index);
    auto state = handle.state.lock();
    if (!state || !state->active || state.get() != &owner)
        throw std::invalid_argument("LambUI: widget binding has expired or belongs to another manager");
    return *handle.widget;
}

template <typename WidgetType>
WidgetType& As(UIWidget& widget) {
    auto* result = dynamic_cast<WidgetType*>(&widget);
    if (!result) throw std::invalid_argument("LambUI: method is not supported by this widget type");
    return *result;
}

AnchorPoint ParseAnchorPoint(const std::string& name) {
    if (name == "TOPLEFT") return AnchorPoint::TopLeft;
    if (name == "TOPRIGHT") return AnchorPoint::TopRight;
    if (name == "BOTTOMLEFT") return AnchorPoint::BottomLeft;
    if (name == "BOTTOMRIGHT") return AnchorPoint::BottomRight;
    if (name == "CENTER") return AnchorPoint::Center;
    if (name == "LEFT") return AnchorPoint::Left;
    if (name == "RIGHT") return AnchorPoint::Right;
    if (name == "TOP") return AnchorPoint::Top;
    if (name == "BOTTOM") return AnchorPoint::Bottom;
    throw std::invalid_argument("LambUI: unknown anchor point '" + name + "'");
}

void PushWidget(lua_State* lua, UIWidget* widget, const std::shared_ptr<BindingState>& owner) {
    LAMBUI_LOGT(TAG, "Expose widget '{}'", widget->GetName());
    auto* memory = lua_newuserdata(lua, sizeof(WidgetHandle));
    new (memory) WidgetHandle{widget, owner};
    luaL_setmetatable(lua, WidgetMetatable);
}

int ReleaseWidget(lua_State* lua) {
    LAMBUI_LOGT(TAG, "Release widget handle");
    auto* handle = static_cast<WidgetHandle*>(lua_touserdata(lua, 1));
    handle->~WidgetHandle();
    return 0;
}

int Dispatch(lua_State* lua) {
    const auto operation = static_cast<Operation>(lua_tointeger(lua, lua_upvalueindex(1)));
    auto owner = Handle(lua, lua_upvalueindex(2)).state.lock();
    if (!owner || !owner->active) throw std::invalid_argument("LambUI: binding has expired");
    LAMBUI_LOGT(TAG, "Lua {}", ToString(operation));

    if (operation == Operation::CreateFrame) {
        const auto type = String(lua, 1);
        const auto name = lua_isnoneornil(lua, 2) ? std::string{} : String(lua, 2);
        auto& parent = lua_isnoneornil(lua, 3) ? owner->manager.GetRoot() : Widget(lua, 3, *owner);
        UIWidget* result = nullptr;
        if (type == "Frame") result = parent.CreateChild<UIWidget>(name);
        else if (type == "Button") result = parent.CreateChild<UIButton>(name);
        else if (type == "StatusBar") result = parent.CreateChild<UISlider>(name);
        else if (type == "EditBox") result = parent.CreateChild<UIInputBox>(name);
        else throw std::invalid_argument("LambUI: unknown frame type '" + type + "'");
        PushWidget(lua, result, owner);
        return 1;
    }

    auto& self = Widget(lua, 1, *owner);
    switch (operation) {
        case Operation::SetSize:
            self.SetSize(Number(lua, 2), Number(lua, 3));
            break;
        case Operation::SetPoint: {
            const auto point = ParseAnchorPoint(String(lua, 2));
            auto* relative = lua_isnil(lua, 3) ? nullptr : &Widget(lua, 3, *owner);
            const auto relativePoint = ParseAnchorPoint(String(lua, 4));
            const float horizontal = Number(lua, 5);
            const float vertical = Number(lua, 6);
            self.SetPoint(point, relative, relativePoint, horizontal, vertical);
            break;
        }
        case Operation::SetAllPoints:
            self.SetAllPoints(lua_isnil(lua, 2) ? nullptr : &Widget(lua, 2, *owner));
            break;
        case Operation::SetVisible:
            if (!lua_isboolean(lua, 2)) throw std::invalid_argument("LambUI: expected a boolean");
            self.SetVisible(lua_toboolean(lua, 2) != 0);
            break;
        case Operation::IsVisible:
            lua_pushboolean(lua, self.IsVisible());
            return 1;
        case Operation::GetName:
            lua_pushlstring(lua, self.GetName().data(), self.GetName().size());
            return 1;
        case Operation::RegisterEvent: {
            const auto name = String(lua, 2);
            if (!lua_isfunction(lua, 3)) throw std::invalid_argument("LambUI: expected an event handler function");
            auto callback = std::make_shared<LuaCallback>(owner, lua, 3);
            owner->manager.SubscribeGameEvent(name, &self, [callback](void* payload) { callback->Invoke(true, payload); });
            break;
        }
        case Operation::CreateTexture:
            PushWidget(lua, self.CreateChild<UITextureWidget>(), owner);
            return 1;
        case Operation::CreateStatusBar:
            PushWidget(lua, self.CreateChild<UISlider>(String(lua, 2)), owner);
            return 1;
        case Operation::CreateFontString:
            PushWidget(lua, self.CreateChild<UITextWidget>(String(lua, 2)), owner);
            return 1;
        case Operation::SetScript: {
            auto& button = As<UIButton>(self);
            const auto name = String(lua, 2);
            UIEventType type;
            if (name == "OnClick") type = UIEventType::OnClick;
            else if (name == "OnEnter") type = UIEventType::OnMouseEnter;
            else if (name == "OnLeave") type = UIEventType::OnMouseLeave;
            else throw std::invalid_argument("LambUI: unknown widget script '" + name + "'");
            if (lua_isnil(lua, 3)) {
                button.RegisterCallback(type, {});
            } else {
                if (!lua_isfunction(lua, 3)) throw std::invalid_argument("LambUI: expected a script function or nil");
                auto callback = std::make_shared<LuaCallback>(owner, lua, 3);
                button.RegisterCallback(type, [callback](const UIEventData&) { callback->Invoke(false); });
            }
            break;
        }
        case Operation::SetTexture:
            As<UITextureWidget>(self).SetTexture(reinterpret_cast<void*>(static_cast<uintptr_t>(Integer(lua, 2))));
            break;
        case Operation::SetTint:
            As<UITextureWidget>(self).SetTint(Color(lua, 2));
            break;
        case Operation::SetMinMaxValues:
            As<UISlider>(self).SetMinMaxValues(Number(lua, 2), Number(lua, 3));
            break;
        case Operation::SetValue:
            As<UISlider>(self).SetValue(Number(lua, 2));
            break;
        case Operation::GetValue:
            lua_pushnumber(lua, As<UISlider>(self).GetValue());
            return 1;
        case Operation::SetText:
            As<UITextWidget>(self).SetText(String(lua, 2));
            break;
        case Operation::SetColor:
            As<UITextWidget>(self).SetColor(Color(lua, 2));
            break;
        default:
            throw std::invalid_argument("LambUI: unknown operation");
    }
    return 0;
}

int ProtectedDispatch(lua_State* lua) {
    try {
        return Dispatch(lua);
    } catch (const std::exception& error) {
        lua_pushstring(lua, error.what());
    } catch (...) {
        lua_pushliteral(lua, "LambUI: unknown C++ exception");
    }
    return lua_error(lua);
}

void PushOperation(lua_State* lua, Operation operation, int rootIndex) {
    LAMBUI_LOGT(TAG, "Register Lua {}", ToString(operation));
    lua_pushinteger(lua, static_cast<lua_Integer>(operation));
    lua_pushvalue(lua, rootIndex);
    lua_pushcclosure(lua, ProtectedDispatch, 2);
}
}

namespace LambUILua {

LuaUIBindings::LuaUIBindings(lua_State* lua, UIManager& manager) {
    LAMBUI_LOGT(TAG, "Construct");
    if (!lua) throw std::invalid_argument("LambUI: Lua state must not be null");
    const int top = lua_gettop(lua);
    const bool mainThread = lua_pushthread(lua) != 0;
    lua_pop(lua, 1);
    if (!mainThread) throw std::invalid_argument("LambUI: bind the main Lua thread");

    lua_rawgetp(lua, LUA_REGISTRYINDEX, &BindingRegistryKey);
    auto* existing = static_cast<WidgetHandle*>(luaL_testudata(lua, -1, WidgetMetatable));
    const bool alreadyBound = existing && !existing->state.expired();
    lua_pop(lua, 1);
    if (alreadyBound) throw std::invalid_argument("LambUI: Lua state already has a binding");

    m_state = std::make_shared<Detail::BindingState>(lua, manager);
    try {
        luaL_newmetatable(lua, WidgetMetatable);
        lua_pushcfunction(lua, ReleaseWidget);
        lua_setfield(lua, -2, "__gc");
        lua_pushliteral(lua, "LambUI widget");
        lua_setfield(lua, -2, "__metatable");

        PushWidget(lua, &manager.GetRoot(), m_state);
        const int rootIndex = lua_gettop(lua);
        lua_pushvalue(lua, rootIndex);
        lua_rawsetp(lua, LUA_REGISTRYINDEX, &BindingRegistryKey);
        lua_newtable(lua);
        const Operation methods[] = {
            Operation::SetSize, Operation::SetPoint, Operation::SetAllPoints, Operation::SetVisible,
            Operation::IsVisible, Operation::GetName, Operation::RegisterEvent, Operation::CreateTexture,
            Operation::CreateStatusBar, Operation::CreateFontString, Operation::SetScript,
            Operation::SetTexture, Operation::SetTint, Operation::SetMinMaxValues, Operation::SetValue,
            Operation::GetValue, Operation::SetText, Operation::SetColor
        };
        for (auto method : methods) {
            PushOperation(lua, method, rootIndex);
            lua_setfield(lua, -2, ToString(method));
        }
        lua_setfield(lua, rootIndex - 1, "__index");

        lua_newtable(lua);
        lua_pushvalue(lua, rootIndex);
        lua_setfield(lua, -2, "Root");
        PushOperation(lua, Operation::CreateFrame, rootIndex);
        lua_setfield(lua, -2, "CreateFrame");
        lua_setglobal(lua, "UI");
    } catch (...) {
        lua_settop(lua, top);
        throw;
    }
    lua_settop(lua, top);
}

LuaUIBindings::~LuaUIBindings() {
    LAMBUI_LOGT(TAG, "Destroy");
    m_state->Detach();
}

}
