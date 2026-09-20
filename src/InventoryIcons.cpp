#include "InventoryIcons.h"
#include "HotbarManager.h"
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace HotbarInventoryIcons {
    void ChordScancodes(const HotbarItemId& id, std::uint32_t& key1, std::uint32_t& key2) {
        key1 = 0;
        key2 = 0;
        const auto* hotkey = HotbarManager::GetSingleton()->FindByItem(id);
        if (!hotkey) return;

        auto keys = hotkey->bind.keys;
        if (!keys.empty()) key1 = keys[0];
        if (keys.size() > 1) key2 = keys[1];
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

            HotbarItemId id;
            id.form = static_cast<RE::FormID>(formIdVal.GetNumber());

            RE::GFxValue enchVal, healthVal;
            if (entry.GetMember("STBench", &enchVal) && enchVal.IsNumber()) id.enchantment = static_cast<RE::FormID>(enchVal.GetNumber());
            if (entry.GetMember("STBhealth", &healthVal) && healthVal.IsNumber()) id.health = static_cast<std::int32_t>(healthVal.GetNumber());

            std::uint32_t key1 = 0, key2 = 0;
            ChordScancodes(id, key1, key2);

            entry.SetMember("hotkeyKey1", RE::GFxValue{ static_cast<double>(key1) });
            entry.SetMember("hotkeyKey2", RE::GFxValue{ static_cast<double>(key2) });
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
        }
    private:
        RE::GFxValue _old;
    };

    void HookMenu(RE::IMenu* a_menu) {
        if (!a_menu || !a_menu->uiMovie) return;

        RE::GFxValue proto;
        if (a_menu->uiMovie->GetVariable(&proto, "_global.InventoryListEntry.prototype") && proto.IsObject()) {
            RE::GFxValue oldFormatName;
            if (proto.GetMember("formatName", &oldFormatName) && oldFormatName.IsObject()) {
                auto impl = RE::make_gptr<FormatNameHook>(std::move(oldFormatName));
                RE::GFxValue newFormatName;
                a_menu->uiMovie->CreateFunction(&newFormatName, impl.get());
                proto.SetMember("formatName", newFormatName);
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
        if (auto menu = ui->GetMenu<RE::InventoryMenu>()) {
            PushKeycaps(menu.get());
        }
    }
}
