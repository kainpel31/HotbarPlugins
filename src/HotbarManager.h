#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <nlohmann/json.hpp> // Tersedia otomatis via CommonLibSSE / nlohmann-json

struct HotbarSlotData {
    std::string name = "Kosong";
    std::uint32_t formID = 0;
    int slotType = 0;
};

class HotbarManager {
public:
    static HotbarManager* GetSingleton() {
        static HotbarManager instance;
        return &instance;
    }

    void Init() {
        std::scoped_lock lk(_lock);
        _slots.resize(24);
        logger::info("HotbarManager initialized with 24 slots.");
    }

    int GetCurrentPreset() const { return _currentPreset; }

    void TogglePreset() {
        std::scoped_lock lk(_lock);
        _currentPreset = (_currentPreset == 1) ? 2 : 1;
        logger::info("Preset switched to: {}", _currentPreset);
        RE::ConsoleLog::GetSingleton()->Print(">>> MMO Hotbar: Beralih ke PRESET %d <<<", _currentPreset);
    }

    std::vector<HotbarSlotData>& GetSlots() {
        return _slots;
    }

    void SaveConfig() {
        std::scoped_lock lk(_lock);
        nlohmann::json j;
        j["currentPreset"] = _currentPreset;
        
        nlohmann::json slotsArray = nlohmann::json::array();
        for (size_t i = 0; i < _slots.size(); ++i) {
            nlohmann::json slotObj;
            slotObj["index"] = i;
            slotObj["name"] = _slots[i].name;
            slotObj["formID"] = _slots[i].formID;
            slotObj["slotType"] = _slots[i].slotType;
            slotsArray.push_back(slotObj);
        }
        j["slots"] = slotsArray;

        std::filesystem::create_directories("Data/SKSE/Plugins");
        std::ofstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (file.is_open()) {
            file << j.dump(4);
            logger::info("Hotbar configuration successfully saved to JSON.");
        }
    }

    void LoadConfig() {
        std::scoped_lock lk(_lock);
        std::ifstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (!file.is_open()) {
            logger::info("No existing configuration JSON found. Starting fresh.");
            return;
        }

        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("currentPreset")) {
                _currentPreset = j["currentPreset"];
            }
            if (j.contains("slots") && j["slots"].is_array()) {
                for (const auto& slotObj : j["slots"]) {
                    int index = slotObj["index"];
                    if (index >= 0 && index < _slots.size()) {
                        _slots[index].name = slotObj["name"];
                        _slots[index].formID = slotObj["formID"];
                        _slots[index].slotType = slotObj["slotType"];
                    }
                }
            }
            logger::info("Hotbar configuration successfully loaded from JSON.");
        } catch (const std::exception& e) {
            logger::error("Failed to parse hotbar JSON configuration: {}", e.what());
        }
    }

    void BindItemFromInventory(int slotIndex) {
        std::scoped_lock lk(_lock);
        if (slotIndex < 0 || slotIndex >= _slots.size()) return;

        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Buka menu Inventory terlebih dahulu!");
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        RE::TESForm* targetForm = nullptr;
        std::string itemName = "Item Terpilih";

        auto equippedRight = player->GetEquippedObject(false);
        if (equippedRight) {
            targetForm = equippedRight;
            itemName = targetForm->GetName();
        }

        if (targetForm) {
            _slots[slotIndex].formID = targetForm->GetFormID();
            _slots[slotIndex].name = itemName.empty() ? "Unnamed" : itemName;
            _slots[slotIndex].slotType = 2;

            SaveConfig(); // Simpan otomatis setiap kali bind berhasil
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar [Preset %d]: Berhasil bind [%s] ke Slot %d!", 
                _currentPreset, _slots[slotIndex].name.c_str(), (slotIndex % 12) + 1);
        } else {
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Pilih item yang valid di inventory!");
        }
    }

    void ExecuteAction(int slotIndex) {
        std::scoped_lock lk(_lock);
        if (slotIndex < 0 || slotIndex >= _slots.size()) return;

        auto& slot = _slots[slotIndex];
        if (slot.formID == 0) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        RE::TESForm* form = RE::TESForm::LookupByID(slot.formID);
        if (!form) return;

        auto equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) return;

        auto boundObj = form->As<RE::TESBoundObject>();
        if (boundObj) {
            equipManager->EquipObject(player, boundObj, nullptr, 1, nullptr, false, false, true, false);
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Meng-equip [%s]", slot.name.c_str());
        }
    }

private:
    mutable std::recursive_mutex _lock;
    std::vector<HotbarSlotData> _slots;
    int _currentPreset = 1;
};
