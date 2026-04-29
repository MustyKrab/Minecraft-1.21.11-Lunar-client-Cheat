#include "SDK.h"
#include <iostream>

// ... other SDK methods ...

void SDK::WorldRenderer::ReloadChunks(JNIEnv* env, jobject levelRenderer) {
    if (!env || !levelRenderer) return;

    jclass clazz = env->GetObjectClass(levelRenderer);
    if (!clazz) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    // 1.21.11 Mapping for chunk reload (Mojmap: allChanged, Intermediary: reload())
    // Try standard Yarn/Intermediary first
    jmethodID reloadMethod = env->GetMethodID(clazz, "reload", "()V");
    
    if (!reloadMethod) {
        env->ExceptionClear(); // Clear the NoSuchMethodError to prevent crash
        
        // Fallback to Mojmap/Forge mapping for 1.21.11
        reloadMethod = env->GetMethodID(clazz, "allChanged", "()V");
        if (!reloadMethod) {
            env->ExceptionClear(); // Clear again
            
            // Fallback to obfuscated SRG if needed (e.g., m_109553_)
            reloadMethod = env->GetMethodID(clazz, "m_109553_", "()V");
            if (!reloadMethod) {
                env->ExceptionClear();
                env->DeleteLocalRef(clazz);
                return; // Abort safely without crashing the game
            }
        }
    }

    // Safely call the method
    env->CallVoidMethod(levelRenderer, reloadMethod);
    
    // Catch any runtime exceptions thrown by the game engine during the reload
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(clazz);
}
