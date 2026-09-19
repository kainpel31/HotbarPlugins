#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

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
        RE::ConsoleLog::GetSingleton()->Print(">>> MMO Hotbar: Beralih ke PRESET %d <<<", _currentPreset);
    }

    std::vector<HotbarSlotData>& GetSlots() { return _slots; }

    // Getter & Setter Konfigurasi Menu
    int GetActiveSlotCount() const { return _activeSlotCount; }
    void SetActiveSlotCount(int count) { _activeSlotCount = count; }

    float GetPosX() const { return _posX; }
    void SetPosX(float x) { _posX = x; }

    float GetPosY() const { return _posY; }
    void SetPosY(float y) { _posY = y; }

    std::uint32_t GetPresetKey() const { return _presetKey; }
    void SetPresetKey(std::uint32_t key) { _presetKey = key; }

    std::uint32_t GetModifierKey() const { return _modifierKey; }
    void SetModifierKey(std::uint32_t key) { _modifierKey = key; }

    void SaveConfig() {
        std::scoped_lock lk(_lock);
        nlohmann::json j;
        j["currentPreset"] = _currentPreset;
        j["activeSlotCount"] = _activeSlotCount;
        j["posX"] = _posX;
        j["posY"] = _posY;
        j["presetKey"] = _presetKey;
        j["modifierKey"] = _modifierKey;
        
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
        }
    }

    void LoadConfig() {
        std::scoped_lock lk(_lock);
        std::ifstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (!file.is_open()) return;

        try {
            nlohmann::json j;
            file >> j;
            if (j.contains("currentPreset")) _currentPreset = j["currentPreset"];
            if (j.contains("activeSlotCount")) _activeSlotCount = j["activeSlotCount"];
            if (j.contains("posX")) _posX = j["posX"];
            if (j.contains("posY")) _posY = j["posY"];
            if (j.contains("presetKey")) _presetKey = j["presetKey"];
            if (j.contains("modifierKey")) _modifierKey = j["modifierKey"];

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
        } catch (...) {}
    }

    void BindItemFromInventory(int slotIndex) {
        std::scoped_lock lk(_lock);
        if (slotIndex < 0 || slotIndex >= _slots.size()) return;

        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) return;

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
            SaveConfig();
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Berhasil bind [%s] ke Slot %d!", _slots[slotIndex].name.c_str(), (slotIndex % 12) + 1);
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
        }
    }

private:
    mutable std::recursive_mutex _lock;
    std::vector<HotbarSlotData> _slots;
    int _currentPreset = 1;
    
    // Pengaturan Menu Baru
    int _activeSlotCount = 12;      // Default 12 slot aktif per preset
    float _posX = 0.5f;             // Persentase Layar X (0.5 = Center)
    float _posY = 0.9f;             // Persentase Layar Y (0.9 = Bottom)
    std::uint32_t _presetKey = 45;  // Default Key 'X' (Scancode 45)
    std::uint32_t _modifierKey = 29;// Default Key 'Left Ctrl' (Scancode 29)
};
