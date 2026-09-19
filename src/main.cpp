#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <cstdint>
#include <string>
#include <vector>

// Struktur Data untuk 24 Slot Hotbar
struct HotbarSlot {
    std::string name = "Kosong";
    std::uint32_t formID = 0;
    int slotType = 0; // 1 = Spell, 2 = Senjata/Armor, 3 = Potion/Item
};

std::vector<HotbarSlot> g_HotbarSlots(24);
int g_CurrentPreset = 1; // 1 untuk Preset 1 (Slot 0-11), 2 untuk Preset 2 (Slot 12-23)
bool g_IsCtrlHeld = false; 
bool g_IsInInventoryMenu = false;

// Fungsi untuk Mengambil Item di Menu Inventory (Binding via Ctrl + Keybind)
void BindItemFromInventory(int slotIndex) {
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
        g_HotbarSlots[slotIndex].formID = targetForm->GetFormID();
        g_HotbarSlots[slotIndex].name = itemName.empty() ? "Unnamed" : itemName;
        g_HotbarSlots[slotIndex].slotType = 2; 

        RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar [Preset %d]: Berhasil bind [%s] ke Slot %d!", 
            g_CurrentPreset, g_HotbarSlots[slotIndex].name.c_str(), (slotIndex % 12) + 1);
    } else {
        RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Pilih item yang valid di inventory!");
    }
}

// Fungsi Eksekusi Hotbar Saat di Luar Menu
void ExecuteHotbarAction(int slotIndex) {
    auto& slot = g_HotbarSlots[slotIndex];
    if (slot.formID == 0) {
        RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar [Preset %d]: Slot %d kosong.", g_CurrentPreset, (slotIndex % 12) + 1);
        return;
    }

    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    RE::TESForm* form = RE::TESForm::LookupByID(slot.formID);
    if (!form) return;

    auto equipManager = RE::ActorEquipManager::GetSingleton();
    if (!equipManager) return;

    auto boundObj = form->As<RE::TESBoundObject>();
    if (boundObj) {
        equipManager->EquipObject(player, boundObj, nullptr, 1, false, false, true, true);
        RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Meng-equip [%s]", slot.name.c_str());
    }
}

// Listener Status Menu Inventory
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
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// Listener Tombol Keyboard Real-Time
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

            // Tombol 'X' ditekan (Click / Toggle) untuk mengganti Preset (ScanCode: 45)
            if (isPressed && key == 45) {
                if (g_CurrentPreset == 1) {
                    g_CurrentPreset = 2;
                    RE::ConsoleLog::GetSingleton()->Print(">>> MMO Hotbar: Beralih ke PRESET 2 (Slot 12-24) <<<");
                } else {
                    g_CurrentPreset = 1;
                    RE::ConsoleLog::GetSingleton()->Print(">>> MMO Hotbar: Beralih ke PRESET 1 (Slot 1-12) <<<");
                }
            }

            // Tombol 'Ctrl' kiri/kanan (ScanCode: 29 atau 157)
            if (key == 29 || key == 157) {
                g_IsCtrlHeld = isPressed;
            }

            // Tombol angka 1 sampai 0, -, = (ScanCode: 2 sampai 13)
            if (isPressed && key >= 2 && key <= 13) {
                int baseSlot = static_cast<int>(key - 2); // 0 - 11
                
                // Tentukan target slot berdasarkan preset aktif (Preset 1: 0-11, Preset 2: 12-23)
                int targetSlot = (g_CurrentPreset == 2) ? (baseSlot + 12) : baseSlot;

                // Jika di menu inventory dan menahan Ctrl -> Lakukan Binding
                if (g_IsInInventoryMenu && g_IsCtrlHeld) {
                    BindItemFromInventory(targetSlot);
                } 
                else if (!g_IsInInventoryMenu) {
                    // Jika di gameplay biasa -> Eksekusi Hotbar
                    ExecuteHotbarAction(targetSlot);
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// Inisialisasi Plugin SKSE
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    auto inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
    if (inputDeviceMgr) {
        inputDeviceMgr->AddEventSink(HotbarInputListener::GetSingleton());
    }

    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(MenuOpenCloseListener::GetSingleton());
    }

    RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar (Toggle Preset X) Berhasil Dimuat!");
    return true;
}
