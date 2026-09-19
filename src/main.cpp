#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <vector>

// Struktur Data untuk 24 Slot Hotbar
struct HotbarSlot {
    std::string name = "Kosong";
    uint32_t formID = 0;
    int slotType = 0;
};

std::vector<HotbarSlot> g_HotbarSlots(24);
bool g_IsPresetModifierHeld = false;

// Fungsi Eksekusi Hotbar
void ExecuteHotbarAction(int slotIndex) {
    auto& slot = g_HotbarSlots[slotIndex];
    if (slot.formID == 0) return;
    
    // Log ke Console Skyrim (Bisa ditekan ` di dalam game)
    RE::ConsoleLog::GetSingleton()->Print("MMO Hotbar: Menjalankan Slot %d", slotIndex + 1);
}

// Listener untuk Menangkap Tombol Keyboard Secara Real-Time
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

            uint32_t key = button->GetIDCode();
            bool isPressed = button->IsPressed();

            // Tombol 'X' sebagai pengubah preset (ScanCode Keyboard: 45)
            if (key == 45) { 
                g_IsPresetModifierHeld = isPressed; 
            }

            // Tombol angka 1 sampai 0, -, = (ScanCode Keyboard: 2 sampai 13)
            if (isPressed && key >= 2 && key <= 13) {
                int targetSlot = (key - 2); // Slot 0 - 11 (Preset 1)
                
                // Jika tombol X ditahan, geser ke Preset 2 (Slot 12 - 23)
                if (g_IsPresetModifierHeld) { 
                    targetSlot += 12; 
                }
                
                ExecuteHotbarAction(targetSlot);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// Inisialisasi Plugin saat Skyrim Dinyalakan
SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    auto inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
    if (inputDeviceMgr) {
        inputDeviceMgr->AddEventSink(HotbarInputListener::GetSingleton());
    }

    return true;
}
