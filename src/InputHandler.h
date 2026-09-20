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
        if (event) {
            if (event->menuName == RE::InventoryMenu::MENU_NAME) {
                _inventoryOpen = event->opening;
            } else if (event->menuName == RE::MagicMenu::MENU_NAME) {
                _magicOpen = event->opening;
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    bool IsInventoryOpen() const { return _inventoryOpen; }
    bool IsMagicOpen() const { return _magicOpen; }

private:
    bool _inventoryOpen = false;
    bool _magicOpen = false;
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
        auto menuState = MenuOpenCloseListener::GetSingleton();
        if (!manager || !menuState) return RE::BSEventNotifyControl::kContinue;

        for (auto* event = *events; event; event = event->next) {
            auto* button = event->AsButtonEvent();
            if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;

            const auto key = button->GetIDCode();

            // 1. Cek Tombol Modifier (misal: Ctrl)
            if (key == manager->GetBindModifierKey()) {
                _modifierHeld = button->IsPressed() || button->IsHeld();
                continue;
            }

            if (!button->IsPressed()) continue;

            // 2. Cek Tombol Preset Toggle
            if (key == manager->GetPresetToggleKey()) {
                manager->TogglePreset();
                continue;
            }

            // 3. Cek Tombol Slot (1 sampai ActiveSlotCount)
            int slot = -1;
            int activeCount = manager->GetActiveSlotCount();
            for (int i = 0; i < activeCount && i < 12; ++i) {
                if (manager->GetSlotKey(i) == key) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) continue;

            const int slotIndex = manager->GetCurrentPreset() == 2 ? slot + 12 : slot;
            
            // 4. Logika Pengecekan Menu & Eksekusi Binding atau Toggle Equip/Unequip
            if (_modifierHeld) {
                if (menuState->IsInventoryOpen()) {
                    manager->BindSelectedInventoryItem(slotIndex);
                } else if (menuState->IsMagicOpen()) {
                    manager->BindSelectedMagicItem(slotIndex);
                }
            } else if (!menuState->IsInventoryOpen() && !menuState->IsMagicOpen()) {
                // Memanggil aksi hotbar (otomatis equip atau unequip jika sudah dipakai)
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
