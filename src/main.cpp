#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "HotbarManager.h"
#include "InputHandler.h"
#include "UIMenu.h"

void InitializeLog() {
    auto path = logger::log_directory();
    if (!path) {
        stl::report_and_fail("Failed to find standard logging directory"sv);
    }
    *path /= fmt::format(FMT_STRING("{}.log"), "MMOHotbar");
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%l] %v"s);
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        logger::info("Skyrim data loaded. Initializing MMOHotbar systems...");
        HotbarManager::GetSingleton()->Init();
        HotbarManager::GetSingleton()->LoadConfig(); // Memuat data dari JSON
        InputHandler::Register();
        UIMenu::Register();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        logger::info("Game loaded/New game started. Ensuring hotbar states are synced.");
        break;
    }
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info) {
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = "MMOHotbar";
    a_info->version = 1;

    if (a_skse->IsEditor()) {
        return false;
    }
    return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v;
    v.PluginVersion(1);
    v.PluginName("MMOHotbar");
    v.AuthorName("Developer");
    v.UsesAddressLibrary(true);
    v.UsesStructsPost629(true);
    return v;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse) {
    InitializeLog();
    logger::info("MMOHotbar plugin loading...");
    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", SKSEMessageHandler)) {
        logger::error("Failed to register SKSE messaging listener.");
        return false;
    }

    return true;
}
