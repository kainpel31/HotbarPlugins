#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSEMenuFramework.h>
#include "HotbarManager.h"

namespace UIMenu {
    void __stdcall RenderSettingsMenu() {
        ImGui::Text("MMO Hotbar - Panel Pengaturan");
        ImGui::Separator();

        // 1. Slider Jumlah Slot Aktif
        int activeCount = HotbarManager::GetSingleton()->GetActiveSlotCount();
        if (ImGui::SliderInt("Jumlah Slot Aktif per Preset", &activeCount, 1, 12)) {
            HotbarManager::GetSingleton()->SetActiveSlotCount(activeCount);
            HotbarManager::GetSingleton()->SaveConfig();
        }

        // 2. Slider Posisi Hotbar X & Y
        float posX = HotbarManager::GetSingleton()->GetPosX();
        float posY = HotbarManager::GetSingleton()->GetPosY();
        if (ImGui::SliderFloat("Posisi Layar X (Kiri-Kanan)", &posX, 0.0f, 1.0f)) {
            HotbarManager::GetSingleton()->SetPosX(posX);
            HotbarManager::GetSingleton()->SaveConfig();
        }
        if (ImGui::SliderFloat("Posisi Layar Y (Atas-Bawah)", &posY, 0.0f, 1.0f)) {
            HotbarManager::GetSingleton()->SetPosY(posY);
            HotbarManager::GetSingleton()->SaveConfig();
        }

        ImGui::Separator();
        ImGui::Text("Pengaturan Tombol (Scancode Key):");
        
        // 3. Rebind Keybind (Preset & Modifier)
        int presetKey = static_cast<int>(HotbarManager::GetSingleton()->GetPresetKey());
        if (ImGui::InputInt("Tombol Toggle Preset (Scancode)", &presetKey)) {
            HotbarManager::GetSingleton()->SetPresetKey(static_cast<std::uint32_t>(presetKey));
            HotbarManager::GetSingleton()->SaveConfig();
        }

        int modKey = static_cast<int>(HotbarManager::GetSingleton()->GetModifierKey());
        if (ImGui::InputInt("Tombol Modifier Ctrl (Scancode)", &modKey)) {
            HotbarManager::GetSingleton()->SetModifierKey(static_cast<std::uint32_t>(modKey));
            HotbarManager::GetSingleton()->SaveConfig();
        }
    }

    void __stdcall RenderHudOverlay() {
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) return;

        auto drawList = ImGui::GetForegroundDrawList();
        ImVec2 displaySize = ImGui::GetIO()->DisplaySize;

        auto manager = HotbarManager::GetSingleton();
        int currentPreset = manager->GetCurrentPreset();
        int activeCount = manager->GetActiveSlotCount();
        auto& slots = manager->GetSlots();

        // Hitung posisi kustom berdasarkan slider X/Y (Default Center Bottom)
        float startX = displaySize.x * manager->GetPosX() - ((activeCount * 45.0f) / 2.0f);
        float startY = displaySize.y * manager->GetPosY();

        // Render kotak visual hotbar sejumlah slot aktif
        for (int i = 0; i < activeCount; ++i) {
            int slotIndex = (currentPreset == 2) ? (i + 12) : i;
            ImVec2 boxMin(startX + (i * 45.0f), startY);
            ImVec2 boxMax(startX + (i * 45.0f) + 40.0f, startY + 40.0f);

            // Warna kotak (Jika terisi vs kosong)
            ImU32 bgColor = (slots[slotIndex].formID != 0) ? IM_COL32(50, 150, 50, 180) : IM_COL32(50, 50, 50, 150);
            drawList->AddRectFilled(boxMin, boxMax, bgColor, 4.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 200), 4.0f);

            // Teks nomor slot
            std::string slotNumText = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 15.0f, boxMin.y + 12.0f), IM_COL32(255, 255, 255, 255), slotNumText.c_str());
        }
    }

    void Register() {
        SKSEMenuFramework::SetSection("MMO Hotbar");
        SKSEMenuFramework::AddSectionItem("Pengaturan Umum & Keybind", RenderSettingsMenu);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);
    }
}
