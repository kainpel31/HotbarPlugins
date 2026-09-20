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
        // Pengaman: Pastikan konteks ImGui aktif sebelum merender menu
        if (!ImGui::GetCurrentContext()) return;

        auto manager = HotbarManager::GetSingleton();
        
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
        if (ImGui::InputInt("Bind modifier key", &modifier)) {
            manager->SetBindModifierKey(static_cast<std::uint32_t>(std::max(modifier, 0)));
            Save();
        }
    }

    inline void __stdcall RenderSlotKeybinds()
    {
        // Pengaman: Pastikan konteks ImGui aktif
        if (!ImGui::GetCurrentContext()) return;

        auto manager = HotbarManager::GetSingleton();
        
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
        // Pengaman berlapis untuk mencegah Access Violation (CTD) pada HUD
        if (!ImGui::GetCurrentContext()) return;
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) return;

        auto manager = HotbarManager::GetSingleton();
        auto* drawList = ImGui::GetForegroundDrawList();
        if (!drawList) return; // Mencegah null pointer dereference

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const int count = manager->GetActiveSlotCount();
        const float width = count * 45.0f;
        
        // Nilai posisi fallback jika belum diatur (di tengah agak ke bawah layar)
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
            
            // Merender kotak hotbar dan nomor slot agar terlihat di layar
            drawList->AddRectFilled(boxMin, boxMax, color, 4.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 200), 4.0f);
            
            const auto text = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 15.0f, boxMin.y + 12.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        }
    }

    inline void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) return;
        
        // Mendaftarkan section dan HUD element dengan benar ke SKSE Menu Framework
        SKSEMenuFramework::AddSectionItem("MMO Hotbar \\ General Settings", RenderGeneralSettings);
        SKSEMenuFramework::AddSectionItem("MMO Hotbar \\ Slot Keybinds", RenderSlotKeybinds);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);
    }
}
