#pragma once

#include "lambui_lua_export.h"
#include <LambUI/UIImageWidget.h>
#include <memory>
#include <functional>
#include <string>

struct lua_State;

namespace LambUI {
class UIManager;
class UIWidget;
}

namespace LambUILua {

namespace Detail { struct BindingState; }

class LAMBUI_LUA_API LuaUIBindings {
public:
    // root defaults to manager.GetRoot(); pass a scene-owned widget to scope
    // UI.Root/CreateFrame's default parent to that subtree instead (e.g. so a
    // per-scene reload can DestroyChildren() on just that subtree).
    LuaUIBindings(lua_State* lua, LambUI::UIManager& manager, LambUI::UIWidget* root = nullptr);
    ~LuaUIBindings();

    void SetImageLoader(LambUI::UIImageLoader loader);
    void SetFontResolver(std::function<void*(const std::string&, int)> resolver);

    LuaUIBindings(const LuaUIBindings&) = delete;
    LuaUIBindings& operator=(const LuaUIBindings&) = delete;

private:
    std::shared_ptr<Detail::BindingState> m_state;
};

} // namespace LambUILua
