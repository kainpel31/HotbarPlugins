#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "HotbarManager.h"

class MenuOpenCloseListener : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static MenuOpenCloseListener* GetSingleton() {
        static MenuOpenCloseListener singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;
        if (a_event->menuName == RE::InventoryMenu::MENU_NAME) {
            g_IsInInventoryMenu = a_event->opening;
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    bool IsInInventory() const { return g_IsInInventoryMenu; }

private:
    bool g_IsInInventoryMenu = false;
};

class HotbarInputListener : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static HotbarInputListener* GetSingleton() {
        static HotbarInputListener singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override {
        if (!a_event || !*a_event) return RE::BSEventNotifyControl::kContinue;

        auto manager = HotbarManager::GetSingleton();

        for (auto event = *a_event; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button) continue;

            std::uint32_t key = button->GetIDCode();
            bool isPressed = button->IsPressed();

            // Cek tombol Toggle Preset dinamis dari menu
            if (isPressed && key == manager->GetPresetKey()) {
                manager->TogglePreset();
            }

            // Cek tombol Modifier Ctrl dinamis dari menu
            if (key == manager->GetModifierKey()) {
                g_IsCtrlHeld = isPressed;
            }

            // Tombol angka 1 sampai 0, -, = (ScanCode: 2 sampai 13)
            if (isPressed && key >= 2 && key <= 13) {
                int baseSlot = static_cast<int>(key - 2);
                
                // Batasi hanya sesuai jumlah slot aktif yang diatur di menu slider
                if (baseSlot >= manager->GetActiveSlotCount()) continue;

                int currentPreset = manager->GetCurrentPreset();
                int targetSlot = (currentPreset == 2) ? (baseSlot + 12) : baseSlot;

                if (MenuOpenCloseListener::GetSingleton()->IsInInventory() && g_IsCtrlHeld) {
                    manager->BindItemFromInventory(targetSlot);
                } 
                else if (!MenuOpenCloseListener::GetSingleton()->IsInInventory()) {
                    manager->ExecuteAction(targetSlot);
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

private:
    bool g_IsCtrlHeld = false;
};

class InputHandler {
public:
    static void Register() {
        auto inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
        if (inputDeviceMgr) {
            inputDeviceMgr->AddEventSink(HotbarInputListener::GetSingleton());
        }

        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(MenuOpenCloseListener::GetSingleton());
        }
    }
};
