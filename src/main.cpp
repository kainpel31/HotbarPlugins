#include <SKSE/SKSE.h>
#include <SKSE/API.h>
#include <RE/Skyrim.h>
#include <SKSE/Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <exception>
#include <memory>

#include "HotbarManager.h"
#include "InputHandler.h"
#include "UIMenu.h"

#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllexport)
#endif

// --- SKSE SERIALIZATION CALLBACKS ---
constexpr std::uint32_t kSerializationID = 'MMOH';

void SaveCallback(SKSE::SerializationInterface* a_intfc)
{
    if (a_intfc->OpenRecord(kSerializationID, 1)) {
        HotbarManager::GetSingleton()->SaveSlotsToSaveGame(a_intfc);
    }
}

void LoadCallback(SKSE::SerializationInterface* a_intfc)
{
    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;
    while (a_intfc->GetNextRecordInfo(type, version, length)) {
        if (type == kSerializationID) {
            HotbarManager::GetSingleton()->LoadSlotsFromSaveGame(a_intfc);
            break;
        }
    }
}

void RevertCallback(SKSE::SerializationInterface* /*a_intfc*/)
{
    HotbarManager::GetSingleton()->Revert();
}
// ------------------------------------

class MenuHookListener : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static MenuHookListener* GetSingleton()
    {
        static MenuHookListener instance;
        return &instance;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (a_event && a_event->opening) {
            auto ui = RE::UI::GetSingleton();
            if (ui) {
                auto menu = ui->GetMenu(a_event->menuName);
                if (menu) {
                    UIMenu::HookMenus(menu.get());
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void InitializeLog()
{
    auto path = SKSE::log::log_directory();
    if (!path) {
        return;
    }
    *path /= "MMOHotbar.log";

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log = std::make_shared<spdlog::logger>("MMOHotbar", std::move(sink));

    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%l] %v");
}

void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
    if (!message) {
        return;
    }

    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        SKSE::log::info("Skyrim data loaded. Initializing MMOHotbar.");
        HotbarManager::GetSingleton()->Init();
        HotbarManager::GetSingleton()->LoadConfig();

        InputHandler::Register();
        UIMenu::Register();

        if (auto ui = RE::UI::GetSingleton()) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(MenuHookListener::GetSingleton());
        }
        break;

    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        SKSE::log::info("Game loaded; MMOHotbar is ready.");
        break;

    default:
        break;
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info)
{
    if (!skse || !info) {
        return false;
    }
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = "MMOHotbar";
    info->version = 1;
    return !skse->IsEditor();
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = [] {
    SKSE::PluginVersionData version;
    version.PluginVersion(1);
    version.PluginName("MMOHotbar");
    version.AuthorName("kainpel31");
    version.UsesAddressLibrary(true);
    version.UsesStructsPost629(true);
    return version;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    InitializeLog();
    SKSE::Init(skse);
    SKSE::log::info("MMOHotbar plugin loading.");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener("SKSE", SKSEMessageHandler)) {
        SKSE::log::error("Failed to register SKSE messaging listener.");
        return false;
    }

    auto serialization = SKSE::GetSerializationInterface();
    if (serialization) {
        serialization->SetUniqueID(kSerializationID);
        serialization->SetSaveCallback(SaveCallback);
        serialization->SetLoadCallback(LoadCallback);
        serialization->SetRevertCallback(RevertCallback);
        SKSE::log::info("Serialization callbacks registered.");
    } else {
        SKSE::log::error("Failed to get SKSE serialization interface.");
    }

    return true;
}
