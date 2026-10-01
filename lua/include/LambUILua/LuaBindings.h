#pragma once

#include "lambui_lua_export.h"
#include <memory>

struct lua_State;

namespace LambUI {
class UIManager;
}

namespace LambUILua {

namespace Detail { struct BindingState; }

class LAMBUI_LUA_API LuaUIBindings {
public:
    LuaUIBindings(lua_State* lua, LambUI::UIManager& manager);
    ~LuaUIBindings();

    LuaUIBindings(const LuaUIBindings&) = delete;
    LuaUIBindings& operator=(const LuaUIBindings&) = delete;

private:
    std::shared_ptr<Detail::BindingState> m_state;
};

} // namespace LambUILua
