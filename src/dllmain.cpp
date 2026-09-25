#include <windows.h>
#include <memory>
#include <string>

#include "../include/scs_telemetry.h"
#include "app_runtime.h"
#include "text_codec.h"
#include "truckersmp_bridge.h"

// ======================== Globals ========================
static HINSTANCE g_hInstance = NULL;
static std::unique_ptr<AppRuntime> g_app;
static std::unique_ptr<TruckersmpBridge> g_tmpBridge;
static scs_log_t g_gameLog = nullptr;

static std::wstring WidenUtf8(const char* value)
{
    if (!value) return L"";
    return text::FromUtf8(value);
}

// ======================== SCS Telemetry SDK Exports ========================

extern "C" __declspec(dllexport) SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t* const params)
{
    if (SCS_GET_MAJOR_VERSION(version) != 1) {
        return SCS_RESULT_unsupported;
    }

    const scs_telemetry_init_params_v101_t* const tp =
        static_cast<const scs_telemetry_init_params_v101_t*>(params);

    g_gameLog = tp->common.log;

    if (g_gameLog) {
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] ===================================");
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] ETS2/ATS TruckersMP Chat Translator v0.3.19");
        std::string versionLog = "[ChatTranslator] Telemetry API version: "
            + std::to_string(SCS_GET_MAJOR_VERSION(version)) + "."
            + std::to_string(SCS_GET_MINOR_VERSION(version));
        g_gameLog(SCS_LOG_TYPE_message, versionLog.c_str());
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] Initializing...");
    }

    if (!g_app) {
        g_app = std::make_unique<AppRuntime>(
            g_hInstance,
            g_gameLog,
            WidenUtf8(tp->common.game_id),
            WidenUtf8(tp->common.game_name));
        g_app->SetRoleResolver([](const std::wstring& name) {
            return g_tmpBridge ? g_tmpBridge->RoleFor(name) : PlayerRole::None;
        });
        g_app->Start();
    }

    if (g_gameLog) {
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] Initialization complete");
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] ===================================");
    }

    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport) SCSAPI_VOID scs_telemetry_shutdown(void)
{
    if (g_gameLog) {
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] Shutting down...");
    }

    if (g_app) {
        g_app->Stop();
        g_app.reset();
    }

    g_gameLog = nullptr;
}

// ======================== TruckersMP Client SDK Exports ========================

extern "C" TMP_EXPORT bool TMP_API truckersmp_init(
    const TruckersMP_Host* host,
    TruckersMP_PluginDesc* desc)
{
    TruckersMP::PluginInfo info;
    info.m_name = "ETS2/ATS Chat Translator";
    info.m_author = "Seven-TMP";
    info.m_version = "0.3.19";
    info.m_description = "Real-time chat translation for TruckersMP.";
    TruckersMP::FillPluginDesc(desc, info);

    if (!g_tmpBridge) {
        g_tmpBridge = std::make_unique<TruckersmpBridge>();
    }
    if (!g_tmpBridge->Start(host)) {
        return false;
    }
    return true;
}

extern "C" TMP_EXPORT void TMP_API truckersmp_shutdown(void)
{
    if (g_tmpBridge) {
        g_tmpBridge->Stop();
    }
}

// ======================== DLL Entry Point ========================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_hInstance = (HINSTANCE)hModule;
        DisableThreadLibraryCalls(hModule);
        break;

    case DLL_PROCESS_DETACH:
        // Do not stop worker/UI threads from DllMain. The loader lock is held here,
        // and waiting on threads can deadlock or crash the host process.
        break;
    }
    return TRUE;
}
