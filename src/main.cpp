#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <SKSE/Logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "HotbarManager.h"
#include "InputHandler.h"
#include "UIMenu.h"

void InitializeLog()
{
    auto path = SKSE::log::log_directory();
    if (!path) {
        stl::report_and_fail("Failed to find standard logging directory"sv);
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
    if (!message) return;
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        SKSE::log::info("Skyrim data loaded. Initializing MMOHotbar.");
        HotbarManager::GetSingleton()->Init();
        HotbarManager::GetSingleton()->LoadConfig();
        InputHandler::Register();
        UIMenu::Register();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        SKSE::log::info("Game loaded; MMOHotbar is ready.");
        break;
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info)
{
    if (!skse || !info) return false;
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
    return true;
}
