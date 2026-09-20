#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <unordered_set>
#include "HotbarManager.h"
#include "HotbarBinding.h"
#include "InventoryIcons.h"

class HotbarInputListener final : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static HotbarInputListener* GetSingleton() {
        static HotbarInputListener singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!events || !*events) return RE::BSEventNotifyControl::kContinue;

        auto manager = HotbarManager::GetSingleton();
        if (!manager) return RE::BSEventNotifyControl::kContinue;

        auto ui = RE::UI::GetSingleton();
        bool inventoryOpen = ui && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME);

        for (auto* event = *events; event; event = event->next) {
            auto* button = event->AsButtonEvent();
            if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;

            const auto key = button->GetIDCode();

            if (button->IsPressed() || button->IsDown()) {
                _heldKeys.insert(key);
            } else if (button->IsUp()) {
                _heldKeys.erase(key);
                continue;
            }

            if (!button->IsPressed() && !button->IsDown()) continue;

            HotbarChord chord;
            chord.device = RE::INPUT_DEVICE::kKeyboard;
            for (auto k : _heldKeys) {
                chord.keys.push_back(k);
            }
            chord.Normalize();

            if (inventoryOpen && _heldKeys.contains(29) && key >= 2 && key <= 13) {
                if (manager->BindSelectedInventoryItem(chord)) {
                    HotbarInventoryIcons::MarkDirty();
                }
            } else if (!inventoryOpen) {
                manager->ExecuteChord(chord);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

private:
    std::unordered_set<std::uint32_t> _heldKeys;
};

class InputHandler {
public:
    static void Register() {
        if (auto input = RE::BSInputDeviceManager::GetSingleton()) {
            input->AddEventSink(HotbarInputListener::GetSingleton());
        }
        HotbarInventoryIcons::Install();
    }
};
