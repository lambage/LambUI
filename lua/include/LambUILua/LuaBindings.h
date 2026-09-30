#pragma once

#include <sol/sol.hpp>

namespace LambUI {
class UIManager;
}

namespace LambUILua {

// Registers the WoW-style `UI.CreateFrame(...)` / `UI.Root` API, bound to a
// specific UIManager instance, into the given sol2 Lua state.
class LuaUIBindings {
public:
    LuaUIBindings(sol::state& lua, LambUI::UIManager& manager);

private:
    void RegisterWidgetTypes(sol::state& lua);
    void RegisterUITable(sol::state& lua);

    LambUI::UIManager& m_manager;
};

} // namespace LambUILua
