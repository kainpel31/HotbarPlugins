#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "HotbarManager.h"

class MenuOpenCloseListener final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    static MenuOpenCloseListener* GetSingleton()
    {
        static MenuOpenCloseListener singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (event && event->menuName == RE::InventoryMenu::MENU_NAME) {
            _inventoryOpen = event->opening;
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    bool IsInventoryOpen() const { return _inventoryOpen; }

private:
    bool _inventoryOpen = false;
};

class HotbarInputListener final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static HotbarInputListener* GetSingleton()
    {
        static HotbarInputListener singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
    {
        if (!events || !*events) return RE::BSEventNotifyControl::kContinue;
        auto manager = HotbarManager::GetSingleton();

        for (auto* event = *events; event; event = event->next) {
            auto* button = event->AsButtonEvent();
            if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;

            const auto key = button->GetIDCode();
            if (key == manager->GetBindModifierKey()) {
                _modifierHeld = button->IsPressed() || button->IsHeld();
                continue;
            }

            if (!button->IsPressed()) continue;

            if (key == manager->GetPresetToggleKey()) {
                manager->TogglePreset();
                continue;
            }

            int slot = -1;
            for (int i = 0; i < 12; ++i) {
                if (manager->GetSlotKey(i) == key) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0 || slot >= manager->GetActiveSlotCount()) continue;

            const int slotIndex = manager->GetCurrentPreset() == 2 ? slot + 12 : slot;
            if (MenuOpenCloseListener::GetSingleton()->IsInventoryOpen() && _modifierHeld) {
                manager->BindItemFromInventory(slotIndex);
            } else if (!MenuOpenCloseListener::GetSingleton()->IsInventoryOpen()) {
                manager->ExecuteAction(slotIndex);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

private:
    bool _modifierHeld = false;
};

class InputHandler {
public:
    static void Register()
    {
        if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
            input->AddEventSink(HotbarInputListener::GetSingleton());
        }
        if (auto ui = RE::UI::GetSingleton()) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(MenuOpenCloseListener::GetSingleton());
        }
    }
};
