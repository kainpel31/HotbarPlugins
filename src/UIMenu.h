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

    // --- Peringatan konflik tombol (mirip dialog "Assign the hotkey anyway?" di STB Hotkey
    // System) -- dipakai bersama oleh panel General Settings & Slot Keybinds. ---
    struct PendingKeyChange {
        bool active = false;
        int target = -100;    // >=0: index slot hotbar (0-11); -1: preset toggle; -2: bind modifier
        std::uint32_t newKey = 0;
        std::string conflictMsg;
    };
    inline PendingKeyChange g_pendingKeyChange;
    constexpr const char* kKeyConflictPopupId = "Konflik Tombol##MMOHotbarKeyConflict";

    inline void ApplyKeyChange(HotbarManager* manager, int target, std::uint32_t key)
    {
        if (target >= 0) manager->SetSlotKey(target, key);
        else if (target == -1) manager->SetPresetToggleKey(key);
        else if (target == -2) manager->SetBindModifierKey(key);
        Save();
    }

    // Dipanggil setiap kali pemain mengubah sebuah input tombol. Kalau bentrok dengan slot
    // lain, tombol toggle/modifier kita sendiri, ATAU kontrol bawaan game (mis. "J" untuk
    // Journal), perubahan TIDAK langsung diterapkan -- munculkan dialog konfirmasi dulu.
    inline void TryAssignKey(HotbarManager* manager, int target, std::uint32_t key, int excludeSlot)
    {
        auto conflict = manager->DescribeKeyConflict(key, excludeSlot);
        if (conflict.empty()) {
            ApplyKeyChange(manager, target, key);
        } else {
            g_pendingKeyChange = PendingKeyChange{ true, target, key, conflict };
            ImGui::OpenPopup(kKeyConflictPopupId);
        }
    }

    // Panggil ini sekali di akhir setiap panel yang memakai TryAssignKey, supaya popup-nya
    // punya tempat untuk digambar pada frame yang sama saat OpenPopup dipanggil.
    inline void RenderKeyConflictPopup(HotbarManager* manager)
    {
        if (ImGui::BeginPopupModal(kKeyConflictPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f), "Tombol sudah digunakan");
            ImGui::Separator();
            ImGui::TextWrapped("%s", g_pendingKeyChange.conflictMsg.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("Tekan tombol ini nanti dan keduanya akan sama-sama aktif. Tetap pasang?");
            ImGui::Spacing();
            if (ImGui::Button("Assign anyway", ImVec2(140, 0))) {
                ApplyKeyChange(manager, g_pendingKeyChange.target, g_pendingKeyChange.newKey);
                g_pendingKeyChange = PendingKeyChange{};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140, 0))) {
                g_pendingKeyChange = PendingKeyChange{};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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

            // PERBAIKAN: tanpa penanda ini, setiap kali menu dibuka ulang Install() akan
            // membungkus 'method' yang sudah pernah di-hook dengan hook baru lagi.
            // Rantai hook lama tidak pernah dilepas (clip Flash menu biasanya tetap hidup
            // di antara buka-tutup), sehingga AddHint() terpanggil berkali-kali tiap update
            // dan hint "Modifier" muncul berduplikasi serta rantai pemanggilan makin panjang.
            std::string hookedFlag = std::string("_mmoHotbarHooked_") + a_target.method;
            RE::GFxValue alreadyHooked;
            if (menuObj.GetMember(hookedFlag.c_str(), &alreadyHooked) && alreadyHooked.IsBool() && alreadyHooked.GetBool()) {
                return;
            }

            RE::GFxValue oldMethod;
            if (!menuObj.GetMember(a_target.method, &oldMethod) || !oldMethod.IsObject()) return;

            auto impl = RE::make_gptr<UpdateHintsHook>(std::move(oldMethod), a_target);
            RE::GFxValue newMethod;
            a_menu->uiMovie->CreateFunction(&newMethod, impl.get());
            menuObj.SetMember(a_target.method, newMethod);
            menuObj.SetMember(hookedFlag.c_str(), RE::GFxValue{ true });
        }
    }

    inline void __stdcall RenderGeneralSettings()
    {
        if (!ImGui::GetCurrentContext()) return;

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return;
        
        ImGui::BeginChild("MMOHotbar_General_Child", ImVec2(0, 450), true);

        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "MMO Hotbar - General Configuration");
        ImGui::Separator();
        ImGui::Spacing();

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
            TryAssignKey(manager, -1, static_cast<std::uint32_t>(std::max(toggle, 0)), -1);
        }

        int modifier = static_cast<int>(manager->GetBindModifierKey());
        if (ImGui::InputInt("Bind Modifier Key (Scan Code)", &modifier)) {
            TryAssignKey(manager, -2, static_cast<std::uint32_t>(std::max(modifier, 0)), -1);
        }

        std::string modName = GetModifierName(manager->GetBindModifierKey());
        std::string infoText = "Inventory/Magic Footer Hint: [" + modName + "] Modifier";
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", infoText.c_str());

        RenderKeyConflictPopup(manager);
        ImGui::EndChild();
    }

    inline void __stdcall RenderSlotKeybinds()
    {
        if (!ImGui::GetCurrentContext()) return;

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return;
        
        ImGui::BeginChild("MMOHotbar_Slots_Child", ImVec2(0, 450), true);

        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "MMO Hotbar - Slot Key Bindings");
        ImGui::Separator();
        ImGui::TextUnformatted("The same slot keys are used by Preset 1 and Preset 2.");
        ImGui::Spacing();

        for (int i = 0; i < 12; ++i) {
            int key = static_cast<int>(manager->GetSlotKey(i));
            std::string label = "Slot " + std::to_string(i + 1);
            if (ImGui::InputInt(label.c_str(), &key)) {
                TryAssignKey(manager, i, static_cast<std::uint32_t>(std::max(key, 0)), i);
            }
        }

        RenderKeyConflictPopup(manager);
        ImGui::EndChild();
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
        if (display.x <= 0.0f || display.y <= 0.0f) return;

        const int count = manager->GetActiveSlotCount();
        float slotSize = 50.0f; 
        float totalWidth = count * slotSize;

        float posX = manager->GetPosX();
        float posY = manager->GetPosY();
        if (posX <= 0.0f) posX = 0.5f;
        if (posY <= 0.0f) posY = 0.9f;

        const float startX = display.x * posX - totalWidth / 2.0f;
        const float startY = display.y * posY;

        const auto& slots = manager->GetSlots();
        const int offset = manager->GetCurrentPreset() == 2 ? 12 : 0;

        for (int i = 0; i < count; ++i) {
            const ImVec2 boxMin(startX + (i * slotSize), startY);
            const ImVec2 boxMax(boxMin.x + 45.0f, boxMin.y + 45.0f);
            
            bool hasItem = false;
            if (slots.size() > static_cast<size_t>(offset + i)) {
                hasItem = (slots[offset + i].formID != 0);
            }

            const auto bgColor = hasItem ? IM_COL32(30, 120, 30, 220) : IM_COL32(40, 40, 40, 200);
            
            drawList->AddRectFilled(boxMin, boxMax, bgColor, 6.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 255), 6.0f, 0, 2.0f);
            
            std::string text = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 16.0f, boxMin.y + 14.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        }
    }

    inline void HookMenus(RE::IMenu* a_menu)
    {
        if (!a_menu) return;
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;

        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) && a_menu == ui->GetMenu(RE::InventoryMenu::MENU_NAME).get()) {
            BottomBarHint::Install(a_menu, BottomBarHint::kItemMenu);
        } else if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) && a_menu == ui->GetMenu(RE::MagicMenu::MENU_NAME).get()) {
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

        SKSE::log::info("SKSEMenuFramework detected. Registering menu sections...");

        // Menyesuaikan dengan standar fungsi kompatibilitas SKSEMenuFramework
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/General Settings", RenderGeneralSettings);
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/Slot Keybinds", RenderSlotKeybinds);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);

        SKSE::log::info("UIMenu registration completed successfully.");
    }
}
