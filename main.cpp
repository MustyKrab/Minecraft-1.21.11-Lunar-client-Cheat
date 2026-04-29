#include <windows.h>
#include <iostream>
#include <thread>
#include "core/JNIHelper.h"
#include "modules/ModuleManager.h"

FILE* f;

void SetupConsole() {
    AllocConsole();
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "[MustyClient] Console allocated. Injecting into JVM..." << std::endl;
}

void MainThread(HMODULE hModule) {
    SetupConsole();

    if (!JNIHelper::Initialize()) {
        std::cout << "[MustyClient] Failed to attach to JVM. Are you running Minecraft?" << std::endl;
        FreeConsole();
        FreeLibraryAndExitThread(hModule, 0);
        return;
    }

    std::cout << "[MustyClient] Successfully attached to JVM!" << std::endl;
    
    ModuleManager::Initialize();

    // Main cheat loop
    while (!GetAsyncKeyState(VK_END)) {
        ModuleManager::OnTick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[MustyClient] Unloading..." << std::endl;
    
    // Cleanup
    JNIHelper::Cleanup();
    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
