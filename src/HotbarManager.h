#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <string>
#include <vector>
#include <mutex>

struct HotbarSlotData {
    std::string name = "Kosong";
    std::uint32_t formID = 0;
    int slotType = 0; // 1 = Spell, 2 = Senjata/Armor, 3 = Potion
};

class HotbarManager {
public:
    static HotbarManager* GetSingleton() {
        static HotbarManager instance;
        return &instance;
    }

    void Init() {
        std::scoped_lock lk(_lock);
        _slots.resize(24); // 24 Slot total (12 Preset 1, 12 Preset 2)
        logger::info("HotbarManager initialized with 24 slots (Thread-Safe).");
    }

    int GetCurrentPreset() const { 
        return _currentPreset; 
    }

    void TogglePreset() {
        std::scoped_lock lk(_lock);
        _currentPreset = (_currentPreset == 1) ? 2 : 1;
        logger::info("Preset switched to: {}", _currentPreset);
        RE::ConsoleLog::GetSingleton()->Print(">>> MMO Hotbar: Beralih ke PRESET %d <<<", _currentPreset);
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
            _slots[slotIndex].slotType = 2; // Senjata/Armor

            logger::info("Bound item [{}] to Slot {} (Preset {})", _slots[slotIndex].name, (slotIndex % 12) + 1, _currentPreset);
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
        if (slot.formID == 0) {
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar [Preset %d]: Slot %d kosong.", _currentPreset, (slotIndex % 12) + 1);
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        RE::TESForm* form = RE::TESForm::LookupByID(slot.formID);
        if (!form) {
            logger::warn("FormID {0:X} pada slot {1} tidak ditemukan di memory game.", slot.formID, slotIndex + 1);
            return;
        }

        auto equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager) return;

        auto boundObj = form->As<RE::TESBoundObject>();
        if (boundObj) {
            equipManager->EquipObject(player, boundObj, nullptr, 1, nullptr, false, false, true, false);
            logger::info("Executed hotbar action for slot {} -> [{}]", slotIndex + 1, slot.name);
            RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Meng-equip [%s]", slot.name.c_str());
        }
    }

private:
    mutable std::recursive_mutex _lock;
    std::vector<HotbarSlotData> _slots;
    int _currentPreset = 1;
};
