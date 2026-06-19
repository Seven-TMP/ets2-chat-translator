#include <windows.h>
#include <memory>
#include <string>

#include "../include/scs_telemetry.h"
#include "app_runtime.h"
#include "text_codec.h"

// ======================== Globals ========================
static HINSTANCE g_hInstance = NULL;
static std::unique_ptr<AppRuntime> g_app;
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
    if (version < SCS_TELEMETRY_VERSION_1_00) {
        return SCS_RESULT_unsupported;
    }

    const scs_telemetry_init_params_v101_t* const tp =
        static_cast<const scs_telemetry_init_params_v101_t*>(params);

    g_gameLog = tp->common.log;

    if (g_gameLog) {
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] ===================================");
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] ETS2/ATS TruckersMP Chat Translator v0.3.17");
        g_gameLog(SCS_LOG_TYPE_message, "[ChatTranslator] Initializing...");
    }

    if (!g_app) {
        g_app = std::make_unique<AppRuntime>(
            g_hInstance,
            g_gameLog,
            WidenUtf8(tp->common.game_id),
            WidenUtf8(tp->common.game_name));
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
