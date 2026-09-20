#pragma once

#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct HotbarChord {
    RE::INPUT_DEVICE device = RE::INPUT_DEVICE::kKeyboard;
    std::vector<std::uint32_t> keys;

    void Normalize() {
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }

    bool operator==(const HotbarChord& rhs) const {
        return device == rhs.device && keys == rhs.keys;
    }

    bool IsSatisfiedBy(const std::unordered_set<std::uint32_t>& held) const {
        if (keys.empty()) return false;
        for (auto key : keys) {
            if (!held.contains(key)) return false;
        }
        return true;
    }
};

struct HotbarItemId {
    RE::FormID form = 0;
    RE::FormID enchantment = 0;
    std::int32_t health = 0;
    std::uint8_t hands = 0;

    bool Same(const HotbarItemId& rhs) const {
        return form == rhs.form && enchantment == rhs.enchantment && health == rhs.health;
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

inline HotbarItemId ReadIdentity(RE::InventoryEntryData* a_entry) {
    HotbarItemId id;
    if (!a_entry) return id;

    id.form = a_entry->object ? a_entry->object->GetFormID() : 0;
    if (!a_entry->extraLists) return id;

    for (auto* extraList : *a_entry->extraLists) {
        if (!extraList) continue;

        if (auto* enchantment = extraList->GetByType<RE::ExtraEnchantment>(); enchantment && enchantment->enchantment) {
            id.enchantment = enchantment->enchantment->GetFormID();
        }

        if (auto* health = extraList->GetByType<RE::ExtraHealth>()) {
            id.health = static_cast<std::int32_t>(std::lround(health->health * 100.0f));
        }
    }
    return id;
}
