#include "LambUILua/LuaBindings.h"

#include "LambUI/LambUI.h"

#include <stdexcept>
#include <unordered_map>

using namespace LambUI;

namespace {

AnchorPoint ParseAnchorPoint(const std::string& name) {
    static const std::unordered_map<std::string, AnchorPoint> kNames = {
        {"TOPLEFT", AnchorPoint::TopLeft},       {"TOPRIGHT", AnchorPoint::TopRight},
        {"BOTTOMLEFT", AnchorPoint::BottomLeft}, {"BOTTOMRIGHT", AnchorPoint::BottomRight},
        {"CENTER", AnchorPoint::Center},         {"LEFT", AnchorPoint::Left},
        {"RIGHT", AnchorPoint::Right},           {"TOP", AnchorPoint::Top},
        {"BOTTOM", AnchorPoint::Bottom},
    };
    auto it = kNames.find(name);
    if (it == kNames.end()) {
        throw std::invalid_argument("LambUI: unknown anchor point '" + name + "'");
    }
    return it->second;
}

} // namespace

namespace LambUILua {

LuaUIBindings::LuaUIBindings(sol::state& lua, UIManager& manager) : m_manager(manager) {
    RegisterWidgetTypes(lua);
    RegisterUITable(lua);
}

void LuaUIBindings::RegisterWidgetTypes(sol::state& lua) {
    lua.new_usertype<UIWidget>("Widget",
        "SetSize", &UIWidget::SetSize,
        "SetPoint", [](UIWidget& self, const std::string& myPoint, UIWidget* relativeTo,
                       const std::string& relativePoint, float xOffset, float yOffset) {
            self.SetPoint(ParseAnchorPoint(myPoint), relativeTo, ParseAnchorPoint(relativePoint), xOffset, yOffset);
        },
        "SetAllPoints", &UIWidget::SetAllPoints,
        "SetVisible", &UIWidget::SetVisible,
        "IsVisible", &UIWidget::IsVisible,
        "GetName", &UIWidget::GetName,
        "RegisterEvent", [this](UIWidget& self, const std::string& eventName, sol::function handler) {
            m_manager.SubscribeGameEvent(eventName, &self, [handler](void* payload) mutable {
                handler(reinterpret_cast<uintptr_t>(payload));
            });
        },
        "CreateTexture", [](UIWidget& self) { return self.CreateChild<UITextureWidget>(); },
        "CreateStatusBar", [](UIWidget& self, const std::string& name) { return self.CreateChild<UISlider>(name); },
        "CreateFontString", [](UIWidget& self, const std::string& name) { return self.CreateChild<UITextWidget>(name); }
    );

    lua.new_usertype<UIButton>("Button", sol::base_classes, sol::bases<UIWidget>(),
        "SetScript", [](UIButton& self, const std::string& eventName, sol::function handler) {
            UIEventType type;
            if (eventName == "OnClick") type = UIEventType::OnClick;
            else if (eventName == "OnEnter") type = UIEventType::OnMouseEnter;
            else if (eventName == "OnLeave") type = UIEventType::OnMouseLeave;
            else throw std::invalid_argument("LambUI: unknown widget script '" + eventName + "'");

            self.RegisterCallback(type, [handler](const UIEventData&) mutable { handler(); });
        }
    );

    lua.new_usertype<UITextureWidget>("Texture", sol::base_classes, sol::bases<UIWidget>(),
        "SetTexture", [](UITextureWidget& self, uint64_t textureId) {
            self.SetTexture(reinterpret_cast<void*>(static_cast<uintptr_t>(textureId)));
        },
        "SetTint", &UITextureWidget::SetTint
    );

    lua.new_usertype<UISlider>("StatusBar", sol::base_classes, sol::bases<UIWidget>(),
        "SetMinMaxValues", &UISlider::SetMinMaxValues,
        "SetValue", &UISlider::SetValue,
        "GetValue", &UISlider::GetValue
    );

    lua.new_usertype<UITextWidget>("FontString", sol::base_classes, sol::bases<UIWidget>(),
        "SetText", &UITextWidget::SetText,
        "SetColor", &UITextWidget::SetColor
    );
}

void LuaUIBindings::RegisterUITable(sol::state& lua) {
    sol::table uiTable = lua.create_named_table("UI");
    uiTable.set("Root", &m_manager.GetRoot());

    uiTable.set_function("CreateFrame", [this](const std::string& widgetType, sol::optional<std::string> name,
                                                sol::optional<UIWidget*> parent) -> UIWidget* {
        UIWidget* parentPtr = parent.value_or(nullptr);
        UIWidget& parentRef = parentPtr ? *parentPtr : m_manager.GetRoot();
        const std::string widgetName = name.value_or(std::string{});

        if (widgetType == "Frame") return parentRef.CreateChild<UIWidget>(widgetName);
        if (widgetType == "Button") return parentRef.CreateChild<UIButton>(widgetName);
        if (widgetType == "StatusBar") return parentRef.CreateChild<UISlider>(widgetName);
        if (widgetType == "EditBox") return parentRef.CreateChild<UIInputBox>(widgetName);

        throw std::invalid_argument("LambUI: unknown frame type '" + widgetType + "'");
    });
}

} // namespace LambUILua
