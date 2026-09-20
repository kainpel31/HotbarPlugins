#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSE/Logger.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>
#include "HotbarBinding.h"

class HotbarManager {
public:
    enum class AssignResult {
        kAdded,
        kReplaced,
        kRemoved
    };

    static HotbarManager* GetSingleton() {
        static HotbarManager instance;
        return &instance;
    }

    void Init() {
        std::scoped_lock lock(_lock);
        _hotkeys.clear();
        _lastActionTime = std::chrono::steady_clock::now();
    }

    const std::vector<HotbarHotkey>& GetHotkeys() const { return _hotkeys; }

    float GetPosX() const { return _posX; }
    void SetPosX(float value) { _posX = std::clamp(value, 0.0f, 1.0f); }
    float GetPosY() const { return _posY; }
    void SetPosY(float value) { _posY = std::clamp(value, 0.0f, 1.0f); }
    int GetActiveSlotCount() const { return _activeSlotCount; }
    void SetActiveSlotCount(int count) { _activeSlotCount = std::clamp(count, 1, 12); }

    bool DetachItem(std::vector<HotbarHotkey>& hotkeys, const HotbarItemId& a_id) {
        bool detached = false;
        for (auto it = hotkeys.begin(); it != hotkeys.end();) {
            std::erase_if(it->items, [&](const HotbarItemId& item) {
                if (item.Same(a_id)) {
                    detached = true;
                    return true;
                }
                return false;
            });
            if (it->items.empty()) {
                it = hotkeys.erase(it);
            } else {
                ++it;
            }
        }
        return detached;
    }

    AssignResult Assign(const HotbarChord& a_bind, const HotbarItemId& a_id) {
        std::scoped_lock lk(_lock);

        for (auto it = _hotkeys.begin(); it != _hotkeys.end(); ++it) {
            if (it->bind == a_bind && it->items.size() == 1 && it->items.front().Same(a_id)) {
                _hotkeys.erase(it);
                SaveConfig();
                return AssignResult::kRemoved;
            }
        }

        bool replacing = std::erase_if(_hotkeys, [&](const HotbarHotkey& h) {
            return h.bind == a_bind;
        }) > 0;

        replacing = DetachItem(_hotkeys, a_id) || replacing;

        _hotkeys.push_back(HotbarHotkey{ a_bind, { a_id } });
        SaveConfig();
        return replacing ? AssignResult::kReplaced : AssignResult::kAdded;
    }

    const HotbarHotkey* FindByItem(const HotbarItemId& a_id) const {
        for (const auto& hotkey : _hotkeys) {
            if (hotkey.Has(a_id)) return &hotkey;
        }
        return nullptr;
    }

    RE::ExtraDataList* FindInstanceList(RE::TESBoundObject* bound, const HotbarItemId& id) {
        if (!bound) return nullptr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* changes = player ? player->GetInventoryChanges() : nullptr;
        if (!changes || !changes->entryList) return nullptr;

        for (auto* entry : *changes->entryList) {
            if (!entry || entry->object != bound || !entry->extraLists) continue;

            for (auto* extraList : *entry->extraLists) {
                if (!extraList) continue;

                RE::FormID enchantment = 0;
                std::int32_t health = 0;

                if (auto* ench = extraList->GetByType<RE::ExtraEnchantment>(); ench && ench->enchantment) {
                    enchantment = ench->enchantment->GetFormID();
                }
                if (auto* h = extraList->GetByType<RE::ExtraHealth>()) {
                    health = static_cast<std::int32_t>(std::lround(h->health * 100.0f));
                }

                if (enchantment == id.enchantment && health == id.health) {
                    return extraList;
                }
            }
        }
        return nullptr;
    }

    bool BindSelectedInventoryItem(const HotbarChord& a_chord) {
        std::scoped_lock lock(_lock);
        auto ui = RE::UI::GetSingleton();
        if (!ui) return false;

        RE::InventoryEntryData* entryData = nullptr;
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            auto menu = ui->GetMenu<RE::InventoryMenu>();
            if (menu && menu->GetRuntimeData().itemList) {
                auto* selected = menu->GetRuntimeData().itemList->GetSelectedItem();
                if (selected && selected->data.objDesc) {
                    entryData = selected->data.objDesc; // Diperbaiki dari .get() ke pointer langsung
                }
            }
        }

        if (!entryData) return false;
        HotbarItemId itemId = ReadIdentity(entryData);
        if (itemId.form == 0) return false;

        Assign(a_chord, itemId);
        return true;
    }

    void SaveConfig() {
        std::scoped_lock lock(_lock);
        nlohmann::json json;
        json["posX"] = _posX;
        json["posY"] = _posY;
        json["activeSlotCount"] = _activeSlotCount;

        auto arr = nlohmann::json::array();
        for (const auto& hb : _hotkeys) {
            nlohmann::json itemObj;
            itemObj["keys"] = hb.bind.keys;
            auto itemsArr = nlohmann::json::array();
            for (const auto& item : hb.items) {
                itemsArr.push_back({{"form", item.form}, {"ench", item.enchantment}, {"health", item.health}});
            }
            itemObj["items"] = itemsArr;
            arr.push_back(itemObj);
        }
        json["bindings"] = arr;

        std::filesystem::create_directories("Data/SKSE/Plugins");
        std::ofstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (file) file << json.dump(4);
    }

    void LoadConfig() {
        std::scoped_lock lock(_lock);
        std::ifstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (!file) return;
        try {
            nlohmann::json json;
            file >> json;
            _posX = std::clamp(json.value("posX", 0.5f), 0.0f, 1.0f);
            _posY = std::clamp(json.value("posY", 0.9f), 0.0f, 1.0f);
            _activeSlotCount = std::clamp(json.value("activeSlotCount", 12), 1, 12);

            if (json.contains("bindings") && json["bindings"].is_array()) {
                _hotkeys.clear();
                for (const auto& bObj : json["bindings"]) {
                    HotbarHotkey hb;
                    if (bObj.contains("keys") && bObj["keys"].is_array()) {
                        hb.bind.keys = bObj["keys"].get<std::vector<std::uint32_t>>();
                        hb.bind.Normalize();
                    }
                    if (bObj.contains("items") && bObj["items"].is_array()) {
                        for (const auto& iObj : bObj["items"]) {
                            HotbarItemId id;
                            id.form = iObj.value("form", 0u);
                            id.enchantment = iObj.value("ench", 0u);
                            id.health = iObj.value("health", 0);
                            hb.items.push_back(id);
                        }
                    }
                    if (!hb.bind.keys.empty() && !hb.items.empty()) {
                        _hotkeys.push_back(hb);
                    }
                }
            }
        } catch (...) {}
    }

    void ExecuteChord(const HotbarChord& a_chord) {
        std::scoped_lock lock(_lock);

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastActionTime).count() < 250) return;
        _lastActionTime = now;

        const HotbarHotkey* matched = nullptr;
        for (const auto& hb : _hotkeys) {
            if (hb.bind == a_chord) {
                matched = &hb;
                break;
            }
        }
        if (!matched || matched->items.empty()) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        auto equipManager = RE::ActorEquipManager::GetSingleton();
        if (!player || !equipManager) return;

        const auto& item = matched->items.front();
        auto* form = RE::TESForm::LookupByID(item.form);
        if (!form) return;

        if (auto* consumable = form->As<RE::AlchemyItem>()) {
            player->DrinkPotion(consumable, nullptr);
            return;
        }
        if (auto* spell = form->As<RE::SpellItem>()) {
            auto l = player->GetEquippedObject(true);
            auto r = player->GetEquippedObject(false);
            if (l == spell || r == spell) {
                equipManager->UnequipObject(player, spell, nullptr, 1, nullptr, false);
            } else {
                equipManager->EquipSpell(player, spell, nullptr);
            }
            return;
        }
        if (auto* shout = form->As<RE::TESShout>()) {
            equipManager->EquipShout(player, shout);
            return;
        }
        if (auto* boundObject = form->As<RE::TESBoundObject>()) {
            bool isEquipped = false;
            RE::ExtraDataList* extraList = FindInstanceList(boundObject, item);

            if (player->GetEquippedObject(true) == boundObject || player->GetEquippedObject(false) == boundObject) {
                isEquipped = true;
            } else if (extraList) {
                if (extraList->HasType(RE::ExtraDataType::kWorn) || extraList->HasType(RE::ExtraDataType::kWornLeft)) {
                    isEquipped = true;
                }
            }

            if (isEquipped) {
                equipManager->UnequipObject(player, boundObject, extraList, 1, nullptr, false);
            } else {
                equipManager->EquipObject(player, boundObject, extraList, 1, nullptr, false, false, true, false);
            }
        }
    }

    // Fungsi stub tambahan agar kompatibel dengan pemanggilan save/load di main.cpp
    void SaveSlotsToSaveGame(SKSE::SerializationInterface*) {}
    void LoadSlotsFromSaveGame(SKSE::SerializationInterface*) {}
    void Revert(SKSE::SerializationInterface*) { Init(); }

private:
    mutable std::recursive_mutex _lock;
    std::vector<HotbarHotkey> _hotkeys;
    float _posX = 0.5f;
    float _posY = 0.9f;
    int _activeSlotCount = 12;
    std::chrono::steady_clock::time_point _lastActionTime{ std::chrono::steady_clock::now() };
};
