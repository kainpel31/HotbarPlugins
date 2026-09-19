#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSEMenuFramework.h> // Disediakan oleh SKSE Menu Framework
#include "HotbarManager.h"

namespace UIMenu {
    // Fungsi untuk merender panel pengaturan di menu SKSE Menu Framework
    void __stdcall RenderSettingsMenu() {
        ImGui::Text("MMO Hotbar - Pengaturan & Manajemen Slot (24 Slot)");
        ImGui::Separator();

        int currentPreset = HotbarManager::GetSingleton()->GetCurrentPreset();
        ImGui::Text("Preset Aktif: Preset %d", currentPreset);
        
        if (ImGui::Button("Ganti Preset (Toggle)")) {
            HotbarManager::GetSingleton()->TogglePreset();
        }

        ImGui::Spacing();
        ImGui::Text("Daftar Slot Hotbar:");

        auto& slots = HotbarManager::GetSingleton()->GetSlots();
        for (int i = 0; i < slots.size(); ++i) {
            int presetNum = (i < 12) ? 1 : 2;
            int displaySlot = (i % 12) + 1;
            
            std::string label = "Preset " + std::to_string(presetNum) + " - Slot " + std::to_string(displaySlot);
            std::string itemInfo = "[" + slots[i].name + "] (ID: " + fmt::format("{:08X}", slots[i].formID) + ")";

            ImGui::BulletText("%s: %s", label.c_str(), itemInfo.c_str());
        }
    }

    // Fungsi untuk merender HUD Overlay visual di layar game
    void __stdcall RenderHudOverlay() {
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
            return;
        }

        auto drawList = ImGui::GetForegroundDrawList();
        ImVec2 displaySize = ImGui::GetIO()->DisplaySize;

        // Tampilkan indikator Preset aktif di pojok kiri atas layar
        int currentPreset = HotbarManager::GetSingleton()->GetCurrentPreset();
        std::string presetText = "MMO Hotbar: [ PRESET " + std::to_string(currentPreset) + " ]";
        
        ImGui::ImDrawListManager::AddText(
            drawList, 
            ImVec2(30, 30), 
            IM_COL32(0, 255, 200, 255), 
            presetText.c_str()
        );
    }

    void Register() {
        // Mendaftarkan menu ke SKSE Menu Framework
        SKSEMenuFramework::SetSection("MMO Hotbar");
        SKSEMenuFramework::AddSectionItem("Pengaturan Hotbar", RenderSettingsMenu);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);
        
        logger::info("SKSE Menu Framework integration registered successfully.");
    }
}
