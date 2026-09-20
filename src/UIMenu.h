#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <imgui.h>
#include <string>
#include "HotbarManager.h"
#include "SKSEMenuFrameworkCompat.h"

namespace UIMenu {
    inline void Save() { HotbarManager::GetSingleton()->SaveConfig(); }

    inline void __stdcall RenderGeneralSettings()
    {
        auto manager = HotbarManager::GetSingleton();
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

        ImGui::Separator();
        ImGui::TextUnformatted("Key values use Skyrim keyboard scan codes.");
        int toggle = static_cast<int>(manager->GetPresetToggleKey());
        if (ImGui::InputInt("Preset toggle key", &toggle)) {
            manager->SetPresetToggleKey(static_cast<std::uint32_t>(std::max(toggle, 0)));
            Save();
        }

        int modifier = static_cast<int>(manager->GetBindModifierKey());
        if (ImGui::InputInt("Bind modifier key", &modifier)) {
            manager->SetBindModifierKey(static_cast<std::uint32_t>(std::max(modifier, 0)));
            Save();
        }
    }

    inline void __stdcall RenderSlotKeybinds()
    {
        auto manager = HotbarManager::GetSingleton();
        ImGui::TextUnformatted("The same slot keys are used by Preset 1 and Preset 2.");
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
        // Pengaman: Pastikan konteks ImGui aktif sebelum merender HUD
        if (!ImGui::GetCurrentContext()) return;
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) return;

        auto manager = HotbarManager::GetSingleton();
        const auto& slots = manager->GetSlots();
        if (slots.size() < 24) return;

        auto* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return; // Pengaman pointer null untuk mencegah crash

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const int count = manager->GetActiveSlotCount();
        const int offset = manager->GetCurrentPreset() == 2 ? 12 : 0;
        const float width = count * 45.0f;
        const float startX = display.x * manager->GetPosX() - width / 2.0f;
        const float startY = display.y * manager->GetPosY();

        for (int i = 0; i < count; ++i) {
            const ImVec2 boxMin(startX + i * 45.0f, startY);
            const ImVec2 boxMax(boxMin.x + 40.0f, boxMin.y + 40.0f);
            const auto color = slots[offset + i].formID != 0 ? IM_COL32(50, 150, 50, 180) : IM_COL32(50, 50, 50, 150);
            drawList->AddRectFilled(boxMin, boxMax, color, 4.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 200), 4.0f);
            const auto text = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 15.0f, boxMin.y + 12.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        }
    }

    inline void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) return;
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/General Settings", RenderGeneralSettings);
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/Slot Keybinds", RenderSlotKeybinds);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);
    }
}
