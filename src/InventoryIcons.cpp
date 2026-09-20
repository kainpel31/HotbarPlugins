#include "InventoryIcons.h"
#include "HotbarManager.h"
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <string>

// CATATAN PERBAIKAN:
// Versi sebelumnya bergantung pada tipe HotbarItemId/HotbarHotkey (HotbarBinding.h)
// dan HotbarManager::FindByItem(), yang tidak pernah ada di HotbarManager yang
// sebenarnya (HotbarManager menyimpan slot berbasis index+formID, bukan chord
// identity berbasis form/enchant/health). Akibatnya file ini gagal dikompilasi.
// Di bawah ini ditulis ulang agar memakai API HotbarManager yang sesungguhnya:
// HotbarManager::FindActiveSlotForForm() + GetSlotKey().

namespace HotbarInventoryIcons {
    // Mencari 1 tombol slot (scan code) yang akan langsung mengaktifkan form ini
    // pada preset hotbar yang sedang aktif. Mengembalikan 0 bila tidak terikat.
    std::uint32_t ScancodeForForm(std::uint32_t a_formID, int& a_outSlotNumber) {
        a_outSlotNumber = 0;
        if (a_formID == 0) return 0;

        auto* manager = HotbarManager::GetSingleton();
        if (!manager) return 0;

        const int slot = manager->FindActiveSlotForForm(a_formID);
        if (slot < 0) return 0;

        a_outSlotNumber = slot + 1; // tampilkan sebagai 1-12, bukan 0-11
        return manager->GetSlotKey(slot);
    }

    void PushKeycaps(RE::IMenu* a_menu) {
        if (!a_menu || !a_menu->uiMovie) return;

        RE::GFxValue itemList;
        if (!a_menu->uiMovie->GetVariable(&itemList, "_root.Menu_mc.inventoryLists.panelContainer.itemList") || !itemList.IsObject()) {
            return;
        }

        RE::GFxValue entryList;
        if (!itemList.GetMember("_entryList", &entryList) || !entryList.IsArray()) {
            return;
        }

        const auto size = entryList.GetArraySize();
        bool changed = false;

        for (std::uint32_t i = 0; i < size; ++i) {
            RE::GFxValue entry;
            if (!entryList.GetElement(i, &entry) || !entry.IsObject()) continue;

            RE::GFxValue formIdVal;
            if (!entry.GetMember("formId", &formIdVal) || !formIdVal.IsNumber()) continue;

            const auto formID = static_cast<std::uint32_t>(formIdVal.GetNumber());

            int slotNumber = 0;
            const std::uint32_t key1 = ScancodeForForm(formID, slotNumber);

            entry.SetMember("hotkeyKey1", RE::GFxValue{ static_cast<double>(key1) });
            entry.SetMember("hotkeySlot", RE::GFxValue{ static_cast<double>(slotNumber) });
            changed = true;
        }

        if (changed) {
            itemList.Invoke("UpdateList");
        }
    }

    class FormatNameHook : public RE::GFxFunctionHandler {
    public:
        FormatNameHook(RE::GFxValue a_old) : _old(std::move(a_old)) {}

        void Call(Params& a_params) override {
            _old.Invoke("call", a_params.retVal, a_params.argsWithThisRef, a_params.argCount + 1);

            // Tambahkan label keycap "[N]" ke nama item bila item ini terikat ke
            // salah satu slot hotbar yang sedang aktif. Sebelumnya hook ini hanya
            // meneruskan hasil formatName asli tanpa pernah menambahkan label,
            // sehingga fitur "keycaps binding slot 1 =[1]" di README tidak pernah muncul.
            if (!a_params.thisPtr || !a_params.movie || !a_params.retVal || !a_params.retVal->IsString()) {
                return;
            }

            RE::GFxValue formIdVal;
            if (!a_params.thisPtr->GetMember("formId", &formIdVal) || !formIdVal.IsNumber()) {
                return;
            }

            const auto formID = static_cast<std::uint32_t>(formIdVal.GetNumber());
            int slotNumber = 0;
            ScancodeForForm(formID, slotNumber);
            if (slotNumber <= 0) return;

            std::string name = a_params.retVal->GetString();
            name += " [" + std::to_string(slotNumber) + "]";

            RE::GFxValue newName;
            a_params.movie->CreateString(&newName, name.c_str());
            *a_params.retVal = newName;
        }
    private:
        RE::GFxValue _old;
    };

    void HookMenu(RE::IMenu* a_menu) {
        if (!a_menu || !a_menu->uiMovie) return;

        RE::GFxValue proto;
        if (a_menu->uiMovie->GetVariable(&proto, "_global.InventoryListEntry.prototype") && proto.IsObject()) {
            // PERBAIKAN: "_global.InventoryListEntry.prototype" adalah objek AS2 global yang
            // dipakai bersama oleh semua instance list entry dan tetap ada selama sesi game
            // berjalan. Tanpa penanda ini, setiap kali menu inventory/magic dibuka, formatName
            // dibungkus hook baru lagi -> rantai hook memanjang tanpa batas dan setiap nama item
            // akan diformat berkali-kali (label "[N]" bisa ditambahkan berulang).
            RE::GFxValue alreadyHooked;
            const bool hooked = proto.GetMember("_mmoHotbarFormatNameHooked", &alreadyHooked)
                && alreadyHooked.IsBool() && alreadyHooked.GetBool();

            if (!hooked) {
                RE::GFxValue oldFormatName;
                if (proto.GetMember("formatName", &oldFormatName) && oldFormatName.IsObject()) {
                    auto impl = RE::make_gptr<FormatNameHook>(std::move(oldFormatName));
                    RE::GFxValue newFormatName;
                    a_menu->uiMovie->CreateFunction(&newFormatName, impl.get());
                    proto.SetMember("formatName", newFormatName);
                    proto.SetMember("_mmoHotbarFormatNameHooked", RE::GFxValue{ true });
                }
            }
        }
        PushKeycaps(a_menu);
    }

    class MenuHookListener : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static MenuHookListener* GetSingleton() {
            static MenuHookListener instance;
            return &instance;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
            if (a_event && a_event->opening) {
                auto ui = RE::UI::GetSingleton();
                if (ui) {
                    auto menu = ui->GetMenu(a_event->menuName);
                    if (menu && (a_event->menuName == RE::InventoryMenu::MENU_NAME || a_event->menuName == RE::MagicMenu::MENU_NAME)) {
                        HookMenu(menu.get());
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void Install() {
        if (auto ui = RE::UI::GetSingleton()) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(MenuHookListener::GetSingleton());
        }
    }

    void MarkDirty() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        // Refresh keduanya: binding bisa dilakukan dari menu inventory ataupun magic.
        if (auto menu = ui->GetMenu<RE::InventoryMenu>()) {
            PushKeycaps(menu.get());
        }
        if (auto menu = ui->GetMenu<RE::MagicMenu>()) {
            PushKeycaps(menu.get());
        }
    }
}
