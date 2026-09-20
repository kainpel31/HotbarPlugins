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

    inline void __stdcall RenderGeneralSettings() {
        SKSEMenuFramework::SyncImGuiContext();
        if (!ImGui::GetCurrentContext()) return;
        
        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "MMO Hotbar - General Configuration");
        ImGui::Separator();
        ImGui::Spacing();

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return;

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
    }

    inline void __stdcall RenderHudOverlay() {
        SKSEMenuFramework::SyncImGuiContext();
        if (!ImGui::GetCurrentContext()) return;
        if (SKSEMenuFramework::IsAnyBlockingWindowOpened()) return;

        auto ui = RE::UI::GetSingleton();
        if (!ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || 
            ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || 
            ui->IsMenuOpen(RE::Console::MENU_NAME)) {
            return;
        }

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return;

        auto* drawList = ImGui::GetBackgroundDrawList(); 
        if (!drawList) return; 

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f) return;

        const int count = manager->GetActiveSlotCount();
        float slotSize = 50.0f; 
        float totalWidth = count * slotSize;

        float posX = manager->GetPosX();
        float posY = manager->GetPosY();

        const float startX = display.x * posX - totalWidth / 2.0f;
        const float startY = display.y * posY;

        for (int i = 0; i < count; ++i) {
            const ImVec2 boxMin(startX + (i * slotSize), startY);
            const ImVec2 boxMax(boxMin.x + 45.0f, boxMin.y + 45.0f);
            
            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(40, 40, 40, 200), 6.0f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 255), 6.0f, 0, 2.0f);
            std::string text = std::to_string(i + 1);
            drawList->AddText(ImVec2(boxMin.x + 16.0f, boxMin.y + 14.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        }
    }

    inline void Register() {
        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }
        SKSEMenuFramework::AddSectionItem("MMO Hotbar/General Settings", RenderGeneralSettings);
        SKSEMenuFramework::AddHudElement(RenderHudOverlay);
    }
}
