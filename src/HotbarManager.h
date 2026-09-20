#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSE/Logger.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory> // Tambahan untuk memproses std::unique_ptr

struct HotbarSlotData {
    std::string name = "Kosong";
    std::string iconPath;
    std::uint32_t formID = 0;
    std::uint32_t formType = 0;
    int slotType = 0;
};

class HotbarManager {
public:
    static HotbarManager* GetSingleton()
    {
        static HotbarManager instance;
        return &instance;
    }

    void Init()
    {
        std::scoped_lock lock(_lock);
        _slots.resize(24);
    }

    int GetCurrentPreset() const { return _currentPreset; }
    void TogglePreset()
    {
        std::scoped_lock lock(_lock);
        _currentPreset = _currentPreset == 1 ? 2 : 1;
        if (auto console = RE::ConsoleLog::GetSingleton()) {
            console->Print(">>> MMO Hotbar: Preset %d <<<", _currentPreset);
        }
    }

    const std::vector<HotbarSlotData>& GetSlots() const { return _slots; }
    int GetActiveSlotCount() const { return _activeSlotCount; }
    void SetActiveSlotCount(int count) { _activeSlotCount = std::clamp(count, 1, 12); }
    float GetPosX() const { return _posX; }
    void SetPosX(float value) { _posX = std::clamp(value, 0.0f, 1.0f); }
    float GetPosY() const { return _posY; }
    void SetPosY(float value) { _posY = std::clamp(value, 0.0f, 1.0f); }
    std::uint32_t GetPresetToggleKey() const { return _presetToggleKey; }
    void SetPresetToggleKey(std::uint32_t key) { _presetToggleKey = key; }
    std::uint32_t GetBindModifierKey() const { return _bindModifierKey; }
    void SetBindModifierKey(std::uint32_t key) { _bindModifierKey = key; }

    std::uint32_t GetSlotKey(int slot) const { return slot >= 0 && slot < 12 ? _slotKeys[slot] : 0; }
    void SetSlotKey(int slot, std::uint32_t key) { if (slot >= 0 && slot < 12) _slotKeys[slot] = key; }

    std::string ResolveIconPath(const RE::TESForm* a_form) const
    {
        if (!a_form) return {};
        std::uint32_t formID = a_form->GetFormID();
        std::string formIDStr = fmt::format("{:08X}", formID);
        std::ifstream iconFile("Data/SKSE/Plugins/I4/IconMapping.json");
        if (iconFile.is_open()) {
            try {
                nlohmann::json j;
                iconFile >> j;
                if (j.contains("icons") && j["icons"].contains(formIDStr)) return j["icons"][formIDStr].get<std::string>();
            } catch (...) {}
        }
        return {};
    }

    bool BindSelectedInventoryItem(int slotIndex)
    {
        std::scoped_lock lock(_lock);
        if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size())) return false;

        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) return false;
        auto menu = ui->GetMenu<RE::InventoryMenu>();
        if (!menu) return false;

        auto* inventoryList = menu->GetRuntimeData().itemList;
        if (!inventoryList) return false;
        auto* selected = inventoryList->GetSelectedItem();
        if (!selected || !selected->data.objDesc) return false;

        auto* form = selected->data.objDesc->GetObject();
        if (!form) return false;
        auto* tesForm = form->As<RE::TESForm>();
        if (!tesForm) return false;

        _slots[slotIndex].formID = tesForm->GetFormID();
        _slots[slotIndex].formType = static_cast<std::uint32_t>(tesForm->GetFormType());
        _slots[slotIndex].name = tesForm->GetName() ? tesForm->GetName() : "Unnamed";
        _slots[slotIndex].iconPath = ResolveIconPath(tesForm);
        _slots[slotIndex].slotType = 0;

        if (menu->uiMovie) {
            RE::GFxValue selectedEntry;
            if (menu->uiMovie->GetVariable(&selectedEntry, "_root.Menu_mc.inventoryLists.itemList.selectedEntry")) {
                if (selectedEntry.IsObject()) {
                    selectedEntry.SetMember("isFavorited", RE::GFxValue(true));
                    selectedEntry.SetMember("hotkey", RE::GFxValue(slotIndex));
                    
                    std::string mappedName = _slots[slotIndex].name + " [Slot " + std::to_string(slotIndex + 1) + "]";
                    selectedEntry.SetMember("text", RE::GFxValue(mappedName.c_str()));
                    
                    menu->uiMovie->Invoke("_root.Menu_mc.inventoryLists.itemList.UpdateList", nullptr, nullptr, 0);
                }
            }
        }
        return true;
    }

    bool BindSelectedMagicItem(int slotIndex)
    {
        std::scoped_lock lock(_lock);
        if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size())) return false;

        auto ui = RE::UI::GetSingleton();
        if (!ui || !ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) return false;
        auto magicMenu = ui->GetMenu<RE::MagicMenu>();
        if (!magicMenu || !magicMenu->uiMovie) return false;

        RE::GFxValue selection;
        if (magicMenu->uiMovie->GetVariable(&selection, "_root.Menu_mc.inventoryLists.itemList.selectedEntry.formId")) {
            if (selection.IsNumber()) {
                std::uint32_t formID = static_cast<std::uint32_t>(selection.GetNumber());
                auto* tesForm = RE::TESForm::LookupByID(formID);
                if (tesForm) {
                    _slots[slotIndex].formID = tesForm->GetFormID();
                    _slots[slotIndex].formType = static_cast<std::uint32_t>(tesForm->GetFormType());
                    _slots[slotIndex].name = tesForm->GetName() ? tesForm->GetName() : "Unnamed Spell";
                    _slots[slotIndex].iconPath = ResolveIconPath(tesForm);
                    _slots[slotIndex].slotType = 0;
                    
                    auto magicFavs = RE::MagicFavorites::GetSingleton();
                    if (magicFavs) {
                        magicFavs->SetFavorite(tesForm);
                    }
                    
                    RE::GFxValue selectedEntry;
                    if (magicMenu->uiMovie->GetVariable(&selectedEntry, "_root.Menu_mc.inventoryLists.itemList.selectedEntry")) {
                        if (selectedEntry.IsObject()) {
                            selectedEntry.SetMember("isFavorited", RE::GFxValue(true));
                            selectedEntry.SetMember("hotkey", RE::GFxValue(slotIndex));
                            
                            std::string mappedName = _slots[slotIndex].name + " [Slot " + std::to_string(slotIndex + 1) + "]";
                            selectedEntry.SetMember("text", RE::GFxValue(mappedName.c_str()));
                            
                            magicMenu->uiMovie->Invoke("_root.Menu_mc.inventoryLists.itemList.UpdateList", nullptr, nullptr, 0);
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    void SaveConfig()
    {
        std::scoped_lock lock(_lock);
        nlohmann::json json;
        json["activeSlotCount"] = _activeSlotCount;
        json["posX"] = _posX;
        json["posY"] = _posY;
        json["presetToggleKey"] = _presetToggleKey;
        json["bindModifierKey"] = _bindModifierKey;
        json["slotKeys"] = _slotKeys;
        
        std::filesystem::create_directories("Data/SKSE/Plugins");
        std::ofstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (file) file << json.dump(4);
    }

    void LoadConfig()
    {
        std::scoped_lock lock(_lock);
        std::ifstream file("Data/SKSE/Plugins/MMOHotbar.json");
        if (!file) return;
        try {
            nlohmann::json json;
            file >> json;
            _activeSlotCount = std::clamp(json.value("activeSlotCount", 12), 1, 12);
            _posX = std::clamp(json.value("posX", 0.5f), 0.0f, 1.0f);
            _posY = std::clamp(json.value("posY", 0.9f), 0.0f, 1.0f);
            _presetToggleKey = json.value("presetToggleKey", 45u);
            _bindModifierKey = json.value("bindModifierKey", 29u);
            if (json.contains("slotKeys") && json["slotKeys"].is_array()) {
                for (std::size_t i = 0; i < _slotKeys.size() && i < json["slotKeys"].size(); ++i)
                    _slotKeys[i] = json["slotKeys"][i].get<std::uint32_t>();
            }
        } catch (...) {}
    }

    void SaveSlotsToSaveGame(SKSE::SerializationInterface* a_intfc)
    {
        std::scoped_lock lock(_lock);
        nlohmann::json j;
        j["currentPreset"] = _currentPreset;
        auto slots = nlohmann::json::array();
        for (std::size_t i = 0; i < _slots.size(); ++i) {
            slots.push_back({
                {"index", i}, {"name", _slots[i].name}, {"formID", _slots[i].formID},
                {"formType", _slots[i].formType}, {"iconPath", _slots[i].iconPath},
                {"slotType", _slots[i].slotType}
            });
        }
        j["slots"] = slots;
        
        std::string dump = j.dump();
        std::uint32_t size = static_cast<std::uint32_t>(dump.size());
        a_intfc->WriteRecordData(&size, sizeof(size));
        a_intfc->WriteRecordData(dump.data(), size);
    }

    void LoadSlotsFromSaveGame(SKSE::SerializationInterface* a_intfc)
    {
        std::scoped_lock lock(_lock);
        std::uint32_t size = 0;
        if (a_intfc->ReadRecordData(&size, sizeof(size)) != sizeof(size)) return;
        
        std::string dump(size, '\0');
        if (a_intfc->ReadRecordData(dump.data(), size) != size) return;
        
        try {
            auto j = nlohmann::json::parse(dump);
            _currentPreset = j.value("currentPreset", 1);
            if (j.contains("slots") && j["slots"].is_array()) {
                for (const auto& slot : j["slots"]) {
                    const auto index = slot.value("index", -1);
                    if (index >= 0 && index < static_cast<int>(_slots.size())) {
                        _slots[index].name = slot.value("name", "Kosong");
                        
                        std::uint32_t oldFormID = slot.value("formID", 0u);
                        std::uint32_t newFormID = 0;
                        if (oldFormID != 0) {
                            if (!a_intfc->ResolveFormID(oldFormID, newFormID)) {
                                newFormID = 0; 
                            }
                        }
                        
                        _slots[index].formID = newFormID;
                        _slots[index].formType = slot.value("formType", 0u);
                        _slots[index].iconPath = slot.value("iconPath", "");
                        _slots[index].slotType = slot.value("slotType", 0);
                    }
                }
            }
        } catch (...) {}
    }

    void Revert()
    {
        std::scoped_lock lock(_lock);
        _currentPreset = 1;
        for (auto& slot : _slots) {
            slot.formID = 0;
            slot.name = "Kosong";
            slot.iconPath = "";
            slot.formType = 0;
        }
    }

    void ExecuteAction(int slotIndex)
    {
        std::scoped_lock lock(_lock);
        if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size())) return;
        auto& slot = _slots[slotIndex];
        if (slot.formID == 0) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        auto form = RE::TESForm::LookupByID(slot.formID);
        auto equipManager = RE::ActorEquipManager::GetSingleton();
        if (!player || !form || !equipManager) return;

        if (auto* consumable = form->As<RE::AlchemyItem>()) {
            player->DrinkPotion(consumable, nullptr);
            return;
        }
        
        if (auto* spell = form->As<RE::SpellItem>()) {
            auto equippedLeft = player->GetEquippedObject(true);
            auto equippedRight = player->GetEquippedObject(false);
            if (equippedLeft == spell || equippedRight == spell) {
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
            RE::ExtraDataList* targetExtraList = nullptr;
            
            auto inventory = player->GetInventory();
            auto it = inventory.find(boundObject);
            
            // Perbaikan C3535: Ekstrak raw pointer dari std::unique_ptr menggunakan .get()
            if (it != inventory.end() && it->second.second) {
                auto* entryData = it->second.second.get(); 
                if (entryData->extraLists) {
                    for (auto* xList : *entryData->extraLists) {
                        if (xList) {
                            if (xList->HasType(RE::ExtraDataType::kWorn) || xList->HasType(RE::ExtraDataType::kWornLeft)) {
                                isEquipped = true;
                                targetExtraList = xList; 
                                break;
                            }
                            if (!targetExtraList) {
                                targetExtraList = xList; 
                            }
                        }
                    }
                }
            } else {
                return; 
            }

            if (!isEquipped && (player->GetEquippedObject(true) == boundObject || player->GetEquippedObject(false) == boundObject)) {
                isEquipped = true;
            }

            if (isEquipped) {
                equipManager->UnequipObject(player, boundObject, targetExtraList, 1, nullptr, false);
            } else {
                equipManager->EquipObject(player, boundObject, targetExtraList, 1, nullptr, false, false, true, false);
            }
        }
    }

private:
    mutable std::recursive_mutex _lock;
    std::vector<HotbarSlotData> _slots;
    std::array<std::uint32_t, 12> _slotKeys{ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
    int _currentPreset = 1;
    int _activeSlotCount = 12;
    float _posX = 0.5f;
    float _posY = 0.9f;
    std::uint32_t _presetToggleKey = 45;
    std::uint32_t _bindModifierKey = 29;
};
