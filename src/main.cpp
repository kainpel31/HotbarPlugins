#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSEMenuFramework.hpp>
#include <vector>

struct HotbarSlot {
    std::string name = "Kosong";
    uint32_t formID = 0;
    int slotType = 0;
};

std::vector<HotbarSlot> g_HotbarSlots(24);
bool g_IsPresetModifierHeld = false;

void ExecuteHotbarAction(int slotIndex) {
    auto& slot = g_HotbarSlots[slotIndex];
    if (slot.formID == 0) return;
    RE::ConsoleLog::GetSingleton()->Print("Menjalankan Slot Hotbar %d", slotIndex + 1);
}

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

            if (key == 45) { g_IsPresetModifierHeld = isPressed; } // Tombol X

            if (isPressed && key >= 2 && key <= 13) {
                int targetSlot = (key - 2);
                if (g_IsPresetModifierHeld) { targetSlot += 12; }
                ExecuteHotbarAction(targetSlot);
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void RenderMMOHotbarMenu() {
    if (ImGui::Begin("MMO Hotbar Configuration")) {
        ImGui::Text("Atur 24 Slot Hotbar Anda di sini.");
        ImGui::End();
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto inputDeviceMgr = RE::BSInputDeviceManager::GetSingleton();
    if (inputDeviceMgr) {
        inputDeviceMgr->AddEventSink(HotbarInputListener::GetSingleton());
    }
    if (SKSEMenuFramework::IsInstalled()) {
        SKSEMenuFramework::AddMenu("MMO Hotbar Config", RenderMMOHotbarMenu);
    }
    return true;
}