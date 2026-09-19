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
            logger::info("Inventory Menu status changed: {}", g_IsInInventoryMenu ? "Opened" : "Closed");
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

        for (auto event = *a_event; event; event = event->next) {
            auto button = event->AsButtonEvent();
            if (!button) continue;

            std::uint32_t key = button->GetIDCode();
            bool isPressed = button->IsPressed();

            // Tombol 'X' (ScanCode: 45) untuk Toggle Preset
            if (isPressed && key == 45) {
                HotbarManager::GetSingleton()->TogglePreset();
            }

            // Tombol 'Ctrl' kiri/kanan (ScanCode: 29 atau 157)
            if (key == 29 || key == 157) {
                g_IsCtrlHeld = isPressed;
            }

            // Tombol angka 1 sampai 0, -, = (ScanCode: 2 sampai 13)
            if (isPressed && key >= 2 && key <= 13) {
                int baseSlot = static_cast<int>(key - 2); // 0 - 11
                int currentPreset = HotbarManager::GetSingleton()->GetCurrentPreset();
                int targetSlot = (currentPreset == 2) ? (baseSlot + 12) : baseSlot;

                if (MenuOpenCloseListener::GetSingleton()->IsInInventory() && g_IsCtrlHeld) {
                    HotbarManager::GetSingleton()->BindItemFromInventory(targetSlot);
                } 
                else if (!MenuOpenCloseListener::GetSingleton()->IsInInventory()) {
                    HotbarManager::GetSingleton()->ExecuteAction(targetSlot);
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
        logger::info("InputHandler event sinks registered successfully.");
    }
};
