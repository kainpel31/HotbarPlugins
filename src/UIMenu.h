#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <imgui.h>
#include <string>
#include "HotbarManager.h"
#include "SKSEMenuFrameworkCompat.h"
#include "SKSE/Logger.h"

namespace UIMenu {
    inline void Save() { HotbarManager::GetSingleton()->SaveConfig(); }

    inline std::string GetModifierName(std::uint32_t keyCode) {
        switch (keyCode) {
            case 0x1D: case 0x9D: return "Ctrl";
            case 0x2A: case 0x36: return "Shift";
            case 0x38: case 0xB8: return "Alt";
            default: return "Key " + std::to_string(keyCode);
        }
    }

    namespace BottomBarHint
    {
        struct Target {
            const char* menuPath;
            const char* method;
            const char* container;
            const char* panel;
            bool selectedArg;
            bool recenter;
        };

        constexpr Target kItemMenu{
            "_root.Menu_mc", "updateBottomBar", "navPanel", nullptr, true, false
        };

        bool AddHint(RE::GFxMovie* a_movie, RE::GFxValue& a_panel, const std::string& a_text, std::uint32_t a_scancode)
        {
            RE::GFxValue data;
            RE::GFxValue controls;
            RE::GFxValue text;
            a_movie->CreateObject(&data);
            a_movie->CreateObject(&controls);
            a_movie->CreateString(&text, a_text.c_str());
            if (!data.IsObject() || !controls.IsObject()) {
                return false;
            }
            controls.SetMember("keyCode", RE::GFxValue{ static_cast<double>(a_scancode) });
            data.SetMember("text", text);
            data.SetMember("controls", controls);

            RE::GFxValue added;
            if (!a_panel.Invoke("addButton", &added, &data, 1)) {
                return false;
            }
            return added.IsObject();
        }

        bool IsHidden(RE::GFxValue& a_obj)
        {
            RE::GFxValue visible;
            return a_obj.GetMember("_visible", &visible) && visible.IsBool() && !visible.GetBool();
        }

        class UpdateHintsHook : public RE::GFxFunctionHandler
        {
        public:
            UpdateHintsHook(RE::GFxValue a_old, const Target& a_target) :
                _old(std::move(a_old)),
                _target(a_target)
            {}

            void Call(Params& a_params) override
            {
                _old.Invoke("call", a_params.retVal, a_params.argsWithThisRef, a_params.argCount + 1);

                if (!a_params.thisPtr || !a_params.movie) {
                    return;
                }

                auto manager = HotbarManager::GetSingleton();
                if (!manager) return;

                RE::GFxValue container;
                if (!a_params.thisPtr->GetMember(_target.container, &container) || !container.IsObject() || IsHidden(container)) {
                    return;
                }
                RE::GFxValue panel = container;
                if (_target.panel && (!container.GetMember(_target.panel, &panel) || !panel.IsObject())) {
                    return;
                }

                std::uint32_t modKey = manager->GetBindModifierKey();
                if (modKey == 0) return;

                std::string hintText = "Modifier (" + GetModifierName(modKey) + ")";
                bool added = AddHint(a_params.movie, panel, hintText, modKey);
                if (!added) {
                    return;
                }

                RE::GFxValue instant{ true };
                panel.Invoke("updateButtons", nullptr, &instant, 1);
            }

        private:
            RE::GFxValue _old;
            const Target& _target;
        };

        inline void Install(RE::IMenu* a_menu, const Target& a_target)
        {
            if (!a_menu || !a_menu->uiMovie) return;

            RE::GFxValue menuObj;
            if (!a_menu->uiMovie->GetVariable(&menuObj, a_target.menuPath) || !menuObj.IsObject()) return;

            RE::GFxValue oldMethod;
            if (!menuObj.GetMember(a_target.method, &oldMethod) || !oldMethod.IsObject()) return;

            auto impl = RE::make_gptr<UpdateHintsHook>(std::move(oldMethod), a_target);
            RE::GFxValue newMethod;
            a_menu->uiMovie->CreateFunction(&newMethod, impl.get());
            menuObj.SetMember(a_target.method, newMethod);
        }
    }

    inline void __stdcall RenderGeneralSettings()
    {
        if (!ImGui::GetCurrentContext()) {
            SKSE::log::warn("RenderGeneralSettings: ImGui context is null!");
            return;
        }

        auto manager = HotbarManager::GetSingleton();
        if (!manager) {
            SKSE::log::error("RenderGeneralSettings: HotbarManager singleton is null!");
            return;
        }
        
        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "MMO Hotbar - General Configuration");
        ImGui::Separator();

        int active = manager->GetActiveSlotCount();
        if (ImGui::SliderInt("Active hotbar slots", &active, 1, 12)) {
            manager->SetActiveSlotCount(active);
            Save();
        }

        float x = manager->GetPosX();
        if (ImGui::SliderFloat("Hotbar position X", &x, 0.0f, 1.0f, "%.2f")) {
            manager->SetPosX(x);
            Save();
        }

        float y = manager->GetPosY();
        if (ImGui::SliderFloat("Hotbar position Y", &y, 0.0f, 1.0f, "%.2f")) {
            manager->SetPosY(y);
            Save();
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Key values use Skyrim keyboard scan codes.");
        
        int toggle = static_cast<int>(manager->GetPresetToggleKey());
        if (ImGui::InputInt("Preset toggle key", &toggle)) {
            manager->SetPresetToggleKey(static_cast<std::uint32_t>(std::max(toggle, 0)));
            Save();
        }

        int modifier = static_cast<int>(manager->GetBindModifierKey());
        if (ImGui::InputInt("Bind Modifier Key (Scan Code)", &modifier)) {
            manager->SetBindModifierKey(static_cast<std::uint32_t>(std::max(modifier, 0)));
            Save();
        }

        std::string modName = GetModifierName(manager->GetBindModifierKey());
        std::string infoText = "Inventory/Magic Footer Hint: [" + modName + "] Modifier";
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", infoText.c_str());
    }

    inline void __stdcall RenderSlotKeybinds()
    {
        if (!ImGui::GetCurrentContext()) {
            SKSE::log::warn("RenderSlotKeybinds: ImGui context is null!");
            return;
        }

        auto manager = HotbarManager::GetSingleton();
        if (!manager) {
            SKSE::log::error("RenderSlotKeybinds: HotbarManager singleton is null!");
            return;
        }
        
        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "MMO Hotbar - Slot Key Bindings");
        ImGui::Separator();
        ImGui::TextUnformatted("The same slot keys are used by Preset 1 and Preset 2.");
        ImGui::Spacing();

        for (int i = 0; i < 12; ++i) {
            int key = static_cast<int>(manager->GetSlotKey(i));
            std::string label = "Slot " + std::to_string(i + 1);
            if (ImGui::InputInt(label.c_str(), &key)) {
                manager->SetSlotKey(i, static_cast<std::uint32_t>(std::max(key, 0)));
                Save();
            }
        }
    }

    inline void __stdcall RenderHudOverlay()
    {
        if (!ImGui::GetCurrentContext()) return;
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) return;

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return;

        auto* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return; 

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const int count = manager->GetActiveSlotCount();
        const float width = count * 45.0f;
        
        float posX = manager->GetPosX();
        float posY = manager->GetPosY();
        if (posX <= 0.0f) posX = 0.5f;
        if (posY <= 0.0f) posY = 0.9f;

        const float startX = display.x * posX - width / 2.0f;
        const float startY = display.y * posY;

        const auto& slots = manager->GetSlots();
        const int offset = manager->GetCurrentPreset() == 2 ? 12 : 0;

        for (int i = 0; i < count; ++i) {
            const ImVec2 boxMin(startX + i * 45.0f, startY);
            const ImVec2 boxMax(boxMin.x + 40.0f, boxMin.y + 40.0f);
            
            bool hasItem = false;
            if (slots.size() > static_cast<size_t>(offset + i)) {
                hasItem = (slots[offset + i].formID != 0);
            }

            const auto color = hasItem ? IM_COL32(50, 150, 50, 180) : IM_COL32(50, 50, 50, 150);
            
            drawList->AddRectFilled(boxMin, boxMax, color, 4.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 200), 4.0f);
            
            const auto text = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 15.0f, boxMin.y + 12.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        }
    }

    inline void HookMenus(RE::IMenu* a_menu)
    {
        if (!a_menu) return;
        if (a_menu->MenuName() == RE::InventoryMenu::MENU_NAME || a_menu->MenuName() == RE::MagicMenu::MENU_NAME) {
            BottomBarHint::Install(a_menu, BottomBarHint::kItemMenu);
        }
    }

    inline void Register()
    {
        SKSE::log::info("UIMenu::Register called.");

        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::error("SKSEMenuFramework is NOT installed or detected! Aborting menu registration.");
            return;
        }

        SKSE::log::info("SKSEMenuFramework detected. Registering menu sections with proper hierarchy...");

        SKSEMenuFramework::AddSectionItem("MMO Hotbar/General Settings", RenderGeneralSettings);
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/Slot Keybinds", RenderSlotKeybinds);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);

        SKSE::log::info("UIMenu registration completed successfully.");
    }
}
