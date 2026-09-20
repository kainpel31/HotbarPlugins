#pragma once

#include <Windows.h>
#include <cstdint>
#include <imgui.h>

namespace SKSEMenuFramework
{
    using RenderFunction = void(__stdcall*)();

    namespace detail
    {
        using AddSectionItemFn = void(*)(const char*, RenderFunction);
        using AddHudElementFn = std::int64_t(*)(RenderFunction);
        using IsAnyBlockingWindowOpenedFn = bool(*)();
        using GetImGuiContextFn = ImGuiContext*(*)(); // Definisi fungsi context

        struct API
        {
            HMODULE module{ nullptr };
            AddSectionItemFn addSectionItem{ nullptr };
            AddHudElementFn addHudElement{ nullptr };
            IsAnyBlockingWindowOpenedFn isAnyBlockingWindowOpened{ nullptr };
            GetImGuiContextFn getImGuiContext{ nullptr }; // Pointer fungsi context
            bool attempted{ false };
        };

        inline API& GetAPI()
        {
            static API api;
            if (!api.attempted) {
                api.attempted = true;
                api.module = GetModuleHandleA("SKSEMenuFramework.dll");
                if (api.module) {
                    api.addSectionItem = reinterpret_cast<AddSectionItemFn>(GetProcAddress(api.module, "AddSectionItem"));
                    api.addHudElement = reinterpret_cast<AddHudElementFn>(GetProcAddress(api.module, "RegisterHudElement"));
                    api.isAnyBlockingWindowOpened = reinterpret_cast<IsAnyBlockingWindowOpenedFn>(GetProcAddress(api.module, "IsAnyBlockingWindowOpened"));
                    api.getImGuiContext = reinterpret_cast<GetImGuiContextFn>(GetProcAddress(api.module, "GetImGuiContext"));
                }
            }
            return api;
        }
    }

    inline bool IsInstalled()
    {
        const auto& api = detail::GetAPI();
        return api.addSectionItem && api.addHudElement;
    }

    // Fungsi untuk menyamakan Context ImGui antara Plugin dan SKSEMenuFramework
    inline void SyncImGuiContext()
    {
        const auto& api = detail::GetAPI();
        if (api.getImGuiContext) {
            ImGui::SetCurrentContext(api.getImGuiContext());
        }
    }

    inline void AddSectionItem(const char* path, RenderFunction renderer)
    {
        const auto& api = detail::GetAPI();
        if (api.addSectionItem) {
            api.addSectionItem(path, renderer);
        }
    }

    inline std::int64_t AddHudElement(RenderFunction renderer)
    {
        const auto& api = detail::GetAPI();
        return api.addHudElement ? api.addHudElement(renderer) : -1;
    }

    inline bool IsAnyBlockingWindowOpened()
    {
        const auto& api = detail::GetAPI();
        return api.isAnyBlockingWindowOpened ? api.isAnyBlockingWindowOpened() : false;
    }
}
