#include <windows.h>
#include <iostream>
#include <thread>
#include <intrin.h>
#include <gl/GL.h>
#pragma comment(lib, "opengl32.lib")

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

// FOX FIX: Thread Hijacking via wglSwapBuffers Hook
// Instead of creating a new thread and attaching it to the JVM (which is instantly flagged),
// we hook the OpenGL swap buffers function. This function is called by the game's main render thread
// every single frame. We execute our cheat logic inside this hook, meaning we are running
// on a legitimate, pre-existing JVM thread.

typedef BOOL(WINAPI* twglSwapBuffers)(HDC hDc);
twglSwapBuffers owglSwapBuffers = nullptr;
bool initialized = false;

BOOL WINAPI hkwglSwapBuffers(HDC hDc) {
    if (!initialized) {
        if (JNIHelper::Initialize()) {
            ModuleManager::Initialize();
            initialized = true;
        }
    }

    if (initialized) {
        // We are now executing on the main Minecraft render thread.
        // No need to call AttachCurrentThread, we are already in the JVM context.
        
        // Only run tick logic if we have the JNIEnv for this thread
        void* envPtr = nullptr;
        if (JNIHelper::vm->GetEnv(&envPtr, JNI_VERSION_1_8) == JNI_OK) {
            JNIHelper::env = (JNIEnv*)envPtr;
            ModuleManager::OnTick();
        }
    }

    return owglSwapBuffers(hDc);
}

// Simple VMT/IAT hook setup for wglSwapBuffers
void SetupHook() {
    HMODULE hOpengl32 = GetModuleHandleA("opengl32.dll");
    if (!hOpengl32) return;

    void* wglSwapBuffersAddr = GetProcAddress(hOpengl32, "wglSwapBuffers");
    if (!wglSwapBuffersAddr) return;

    // Basic inline hook (trampoline)
    // Note: In a real production cheat, use MinHook or a custom disassembler to handle relocations.
    // This is a simplified 64-bit absolute jump hook for demonstration.
    
    DWORD oldProtect;
    VirtualProtect(wglSwapBuffersAddr, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
    
    // Save original bytes for trampoline (simplified, assumes no relative instructions in first 14 bytes)
    owglSwapBuffers = (twglSwapBuffers)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy((void*)owglSwapBuffers, wglSwapBuffersAddr, 14);
    
    // Jump back to original + 14
    BYTE jmpBack[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    *(uintptr_t*)(jmpBack + 6) = (uintptr_t)wglSwapBuffersAddr + 14;
    memcpy((void*)((uintptr_t)owglSwapBuffers + 14), jmpBack, 14);

    // Write jump to our hook
    BYTE jmpToHook[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    *(uintptr_t*)(jmpToHook + 6) = (uintptr_t)hkwglSwapBuffers;
    memcpy(wglSwapBuffersAddr, jmpToHook, 14);
    
    VirtualProtect(wglSwapBuffersAddr, 14, oldProtect, &oldProtect);
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        
        UnlinkPEB(hModule);
        ErasePEHeaders(hModule);

        // Instead of creating a thread, we just place our hook.
        // The cheat will initialize and run automatically on the next frame render.
        SetupHook();
    }
    return TRUE;
}
