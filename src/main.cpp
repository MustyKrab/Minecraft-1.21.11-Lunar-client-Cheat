#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>

#include "core/JNIHelper.h"
#include "modules/ModuleManager.h"

// Custom PEB structures for stealth
typedef struct _UNICODE_STRING_C {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_C;

typedef struct _PEB_LDR_DATA_C {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_C, * PPEB_LDR_DATA_C;

typedef struct _LDR_DATA_TABLE_ENTRY_C {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING_C FullDllName;
    UNICODE_STRING_C BaseDllName;
} LDR_DATA_TABLE_ENTRY_C, * PLDR_DATA_TABLE_ENTRY_C;

typedef struct _PEB_C {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    PPEB_LDR_DATA_C Ldr;
} PEB_C, * PPEB_C;

void ErasePEHeaders(HINSTANCE hModule) {
    DWORD oldProtect;
    VirtualProtect(hModule, 4096, PAGE_READWRITE, &oldProtect);
    SecureZeroMemory(hModule, 4096);
    VirtualProtect(hModule, 4096, oldProtect, &oldProtect);
}

void UnlinkPEB(HINSTANCE hModule) {
#ifdef _WIN64
    PPEB_C pPEB = (PPEB_C)__readgsqword(0x60);
#else
    PPEB_C pPEB = (PPEB_C)__readfsdword(0x30);
#endif

    PPEB_LDR_DATA_C pLoaderData = pPEB->Ldr;
    PLIST_ENTRY listHead = &pLoaderData->InMemoryOrderModuleList;
    PLIST_ENTRY listCurrent = listHead->Flink;

    while (listCurrent != listHead) {
        PLDR_DATA_TABLE_ENTRY_C pEntry = CONTAINING_RECORD(listCurrent, LDR_DATA_TABLE_ENTRY_C, InMemoryOrderLinks);
        if ((HINSTANCE)pEntry->DllBase == hModule) {
            pEntry->InLoadOrderLinks.Blink->Flink = pEntry->InLoadOrderLinks.Flink;
            pEntry->InLoadOrderLinks.Flink->Blink = pEntry->InLoadOrderLinks.Blink;
            
            pEntry->InMemoryOrderLinks.Blink->Flink = pEntry->InMemoryOrderLinks.Flink;
            pEntry->InMemoryOrderLinks.Flink->Blink = pEntry->InMemoryOrderLinks.Blink;
            
            pEntry->InInitializationOrderLinks.Blink->Flink = pEntry->InInitializationOrderLinks.Flink;
            pEntry->InInitializationOrderLinks.Flink->Blink = pEntry->InInitializationOrderLinks.Blink;
            break;
        }
        listCurrent = listCurrent->Flink;
    }
}

DWORD WINAPI CheatThread(LPVOID lParam) {
    if (JNIHelper::Initialize()) {
        ModuleManager::Initialize();
        
        // FIX: Added a way to cleanly exit the cheat thread (e.g. pressing END key)
        while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
            ModuleManager::OnTick();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 TPS
        }
    }
    
    // Clean up allocated memory before exiting
    ModuleManager::Cleanup();
    JNIHelper::Cleanup();
    FreeLibraryAndExitThread((HMODULE)lParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        UnlinkPEB(hModule);
        ErasePEHeaders(hModule);
        
        HANDLE hThread = CreateThread(nullptr, 0, CheatThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
