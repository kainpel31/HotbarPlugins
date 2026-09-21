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
        SKSE::log::info("MMOHotbar initialized with 24 slots.");
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

    std::uint32_t GetSlotKey(int slot) const
    {
        return slot >= 0 && slot < 12 ? _slotKeys[slot] : 0;
    }

    // Mencari slot lokal (0-11) pada preset yang sedang aktif yang terisi oleh form tertentu.
    // Dipakai oleh HotbarInventoryIcons untuk menampilkan keycap "[N]" pada menu inventory/magic.
    // Mengembalikan -1 apabila form tidak terikat ke slot manapun pada preset aktif.
    int FindActiveSlotForForm(std::uint32_t a_formID) const
    {
        std::scoped_lock lock(_lock);
        if (a_formID == 0) return -1;

        const int offset = _currentPreset == 2 ? 12 : 0;
        for (int i = 0; i < _activeSlotCount && i < 12; ++i) {
            const std::size_t index = static_cast<std::size_t>(offset + i);
            if (index < _slots.size() && _slots[index].formID == a_formID) {
                return i;
            }
        }
        return -1;
    }

    void SetSlotKey(int slot, std::uint32_t key)
    {
        if (slot >= 0 && slot < 12) _slotKeys[slot] = key;
    }

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
                if (j.contains("icons") && j["icons"].contains(formIDStr)) {
                    return j["icons"][formIDStr].get<std::string>();
                }
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
        // PERBAIKAN: Menggunakan pointer langsung '->' karena objDesc bertipe pointer mentah RE::InventoryEntryData*
        if (!selected || !selected->data.objDesc) return false;

        auto* entry = selected->data.objDesc;
        auto* form = entry->GetObject();
        if (!form) return false;
        auto* tesForm = form->As<RE::TESForm>();
        if (!tesForm) return false;

        _slots[slotIndex].formID = tesForm->GetFormID();
        _slots[slotIndex].formType = static_cast<std::uint32_t>(tesForm->GetFormType());
        _slots[slotIndex].name = tesForm->GetName() ? tesForm->GetName() : "Unnamed";
        _slots[slotIndex].iconPath = ResolveIconPath(tesForm);
        _slots[slotIndex].slotType = 0;
        SaveConfig();

        // Tandai item sebagai favorite betulan (mekanisme native game, bukan hack UI),
        // supaya bintang favorite vanilla/SkyUI langsung muncul. Dilakukan lewat entry
        // yang sedang benar-benar dipilih di menu (bukan lewat task queue terpisah),
        // sehingga aman dipanggil selagi menu Inventory masih terbuka -- tidak men-desync
        // preview 3D item yang sedang tampil (pola diambil dari STB Hotkey System,
        // Favorites.cpp::FavoriteSelectedItem).
        bool alreadyFav = false;
        RE::ExtraDataList* firstList = nullptr;
        if (entry->extraLists) {
            for (auto* xl : *entry->extraLists) {
                if (!xl) continue;
                if (!firstList) firstList = xl;
                if (xl->HasType(RE::ExtraDataType::kHotkey)) { alreadyFav = true; break; }
            }
        }
        if (!alreadyFav) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (auto* changes = player->GetInventoryChanges()) {
                    changes->SetFavorite(entry, firstList);
                    inventoryList->Update(player);
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
                    SaveConfig();

                    // Mantra/shout tidak lewat InventoryEntryData -- favoritnya disimpan di
                    // MagicFavorites (mekanisme native game yang sama dipakai menu Magic).
                    if (tesForm->Is(RE::FormType::Spell) || tesForm->Is(RE::FormType::Shout)) {
                        if (auto* mf = RE::MagicFavorites::GetSingleton()) {
                            mf->SetFavorite(tesForm);
                        }
                    }
                    // Segarkan list yang sedang terbuka supaya bintang langsung terlihat tanpa
                    // perlu tutup-buka menu (aman: cuma InvalidateData pada list Flash-nya,
                    // tidak menyentuh internal privat/relocation berisiko).
                    RE::GFxValue itemList;
                    if (magicMenu->uiMovie->GetVariable(&itemList, "_root.Menu_mc.inventoryLists.panelContainer.itemList") && itemList.IsObject()) {
                        itemList.Invoke("InvalidateData");
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
        json["currentPreset"] = _currentPreset;
        json["activeSlotCount"] = _activeSlotCount;
        json["posX"] = _posX;
        json["posY"] = _posY;
        json["presetToggleKey"] = _presetToggleKey;
        json["bindModifierKey"] = _bindModifierKey;
        json["slotKeys"] = _slotKeys;
        
        auto slots = nlohmann::json::array();
        for (std::size_t i = 0; i < _slots.size(); ++i) {
            slots.push_back({
                {"index", i}, {"name", _slots[i].name}, {"formID", _slots[i].formID},
                {"formType", _slots[i].formType}, {"iconPath", _slots[i].iconPath},
                {"slotType", _slots[i].slotType}
            });
        }
        json["slots"] = std::move(slots);
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
            _currentPreset = json.value("currentPreset", 1);
            _activeSlotCount = std::clamp(json.value("activeSlotCount", 12), 1, 12);
            _posX = std::clamp(json.value("posX", 0.5f), 0.0f, 1.0f);
            _posY = std::clamp(json.value("posY", 0.9f), 0.0f, 1.0f);
            _presetToggleKey = json.value("presetToggleKey", 45u);
            _bindModifierKey = json.value("bindModifierKey", 29u);
            if (json.contains("slotKeys") && json["slotKeys"].is_array()) {
                for (std::size_t i = 0; i < _slotKeys.size() && i < json["slotKeys"].size(); ++i)
                    _slotKeys[i] = json["slotKeys"][i].get<std::uint32_t>();
            }
            if (json.contains("slots") && json["slots"].is_array()) {
                for (const auto& slot : json["slots"]) {
                    const auto index = slot.value("index", -1);
                    if (index >= 0 && index < static_cast<int>(_slots.size())) {
                        _slots[index].name = slot.value("name", "Kosong");
                        _slots[index].formID = slot.value("formID", 0u);
                        _slots[index].formType = slot.value("formType", 0u);
                        _slots[index].iconPath = slot.value("iconPath", "");
                        _slots[index].slotType = slot.value("slotType", 0);
                    }
                }
            }
        } catch (const std::exception& error) {
            SKSE::log::error("Failed to load MMOHotbar configuration: {}", error.what());
        }
    }

    // Mencari ExtraDataList milik stack yang tepat (dipakai/worn atau tidak) untuk objek ini,
    // supaya EquipObject/UnequipObject menyasar instance yang benar-benar sesuai. Ini pola yang
    // dipakai game sendiri saat toggle lewat Favorites/hotkey vanilla (bukan hasil tebakan --
    // diambil dari source asli STB Hotkey System, EquipDispatch.cpp::StackList).
    // Tanpa extraDataList yang tepat, event OnEquipped/OnUnequipped item bisa tidak menyasar
    // instance yang benar, dan pada beberapa kasus ikut menyumbang desync animasi.
    RE::ExtraDataList* FindStackList(RE::Actor* a_actor, RE::TESBoundObject* a_object, bool a_worn) const
    {
        auto* changes = a_actor->GetInventoryChanges();
        if (!changes || !changes->entryList) return nullptr;
        for (auto* entry : *changes->entryList) {
            if (!entry || entry->object != a_object || !entry->extraLists) continue;
            for (auto* xl : *entry->extraLists) {
                if (!xl) continue;
                const bool worn = xl->HasType(RE::ExtraDataType::kWorn) || xl->HasType(RE::ExtraDataType::kWornLeft);
                if (worn == a_worn) return xl;
            }
        }
        return nullptr;
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
            equipManager->EquipSpell(player, spell, nullptr);
            return;
        }
        if (auto* shout = form->As<RE::TESShout>()) {
            equipManager->EquipShout(player, shout);
            return;
        }
        if (auto* boundObject = form->As<RE::TESBoundObject>()) {
            bool isEquipped = false;
            auto equippedLeft = player->GetEquippedObject(true);
            auto equippedRight = player->GetEquippedObject(false);

            if (equippedLeft == boundObject || equippedRight == boundObject) {
                isEquipped = true;
            }

            if (isEquipped) {
                // PERBAIKAN BUG UTAMA (animasi tidak kembali kosong saat unequip):
                // 1. Sertakan ExtraDataList stack yang benar-benar "worn" (bukan nullptr),
                //    persis seperti yang dilakukan game sendiri saat toggle Favorites vanilla.
                // 2. Panggil Update3DModel() setelah UnequipObject. Ini yang sebelumnya HILANG.
                //    Skyrim TIDAK otomatis menyegarkan model 3D & state animasi aktor setelah
                //    equip/unequip "immediate" (non-queued, queueEquip=false) yang dipicu di
                //    luar alur menu Favorites bawaan -- itu sebabnya mesh menghilang tapi
                //    behavior graph (animasi "pedang di tangan") tetap membaca state lama.
                //    Game asli SELALU memanggil Update3DModel tepat setelah equip/unequip
                //    immediate (lihat FavoritesMenu::UseQuickslotItem / hotkey 1-8 vanilla).
                auto* xl = FindStackList(player, boundObject, true);
                equipManager->UnequipObject(player, boundObject, xl, 1, nullptr, false, false, true, false);
            } else {
                auto* xl = FindStackList(player, boundObject, false);
                equipManager->EquipObject(player, boundObject, xl, 1, nullptr, false, false, true, false);
            }

            if (auto* process = player->GetActorRuntimeData().currentProcess) {
                process->Update3DModel(player);
            }
        }
    }

    // PERINGATAN KONFLIK TOMBOL (mirip dialog "Key X is already bound to..." di STB Hotkey
    // System). Mengecek dua sumber konflik:
    //   (a) slot lain di hotbar kita sendiri, plus tombol toggle-preset & modifier-bind kita,
    //   (b) kontrol bawaan game (mis. tombol "J" untuk Journal) lewat ControlMap milik game --
    //       reverse-lookup asli, bukan tabel tebakan sendiri.
    // Mengembalikan string kosong jika tidak ada konflik, atau pesan penjelasan jika ada.
    // a_excludeSlot: indeks slot (0-11) yang sedang diedit, supaya tidak dianggap "bentrok
    // dengan dirinya sendiri"; isi -1 saat mengecek tombol toggle-preset/modifier.
    std::string DescribeKeyConflict(std::uint32_t a_scancode, int a_excludeSlot = -1) const
    {
        if (a_scancode == 0) return {};

        for (int i = 0; i < 12; ++i) {
            if (i == a_excludeSlot) continue;
            if (_slotKeys[i] == a_scancode) {
                return "Sudah dipakai oleh Slot " + std::to_string(i + 1);
            }
        }
        if (_presetToggleKey == a_scancode) {
            return "Sudah dipakai oleh 'Toggle Preset'";
        }
        if (_bindModifierKey == a_scancode) {
            return "Sudah dipakai oleh 'Bind Modifier'";
        }

        if (auto* controlMap = RE::ControlMap::GetSingleton()) {
            const auto name = controlMap->GetUserEventName(
                a_scancode, RE::INPUT_DEVICE::kKeyboard, RE::UserEvents::INPUT_CONTEXT_ID::kGameplay);
            if (!name.empty()) {
                return "Sudah dipakai kontrol game: \"" + std::string(name) + "\" -- keduanya akan sama-sama aktif saat ditekan.";
            }
        }
        return {};
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
