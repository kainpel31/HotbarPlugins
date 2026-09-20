#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <vector>
#include <cstdint>
#include <algorithm>

struct HotbarItemId {
    std::uint32_t form{ 0 };
    std::uint32_t enchantment{ 0 };
    std::int32_t health{ 0 };

    bool Same(const HotbarItemId& a_other) const {
        return form == a_other.form && enchantment == a_other.enchantment && health == a_other.health;
    }
};

struct HotbarChord {
    RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kKeyboard }; // Ditambahkan agar cocok dengan InputHandler.h
    std::vector<std::uint32_t> keys;

    void Normalize() {
        std::sort(keys.begin(), keys.end());
    }

    bool operator==(const HotbarChord& a_other) const {
        return device == a_other.device && keys == a_other.keys;
    }
};

struct HotbarHotkey {
    HotbarChord bind;
    std::vector<HotbarItemId> items;

    bool Has(const HotbarItemId& a_id) const {
        for (const auto& item : items) {
            if (item.Same(a_id)) return true;
        }
        return false;
    }
};

inline HotbarItemId ReadIdentity(RE::InventoryEntryData* a_entryData) {
    HotbarItemId id;
    if (!a_entryData || !a_entryData->object) return id;

    id.form = a_entryData->object->GetFormID();

    if (auto* extraLists = a_entryData->extraLists) {
        for (auto* extraList : *extraLists) {
            if (!extraList) continue;

            if (auto* ench = extraList->GetByType<RE::ExtraEnchantment>(); ench && ench->enchantment) {
                id.enchantment = ench->enchantment->GetFormID();
            }
            if (auto* h = extraList->GetByType<RE::ExtraHealth>()) {
                id.health = static_cast<std::int32_t>(std::lround(h->health * 100.0f));
            }
            break;
        }
    }
    return id;
}
