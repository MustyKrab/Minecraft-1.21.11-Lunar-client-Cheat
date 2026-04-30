#include "AutoClicker.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <chrono>
#include <random>
#include <windows.h>
#include <cmath>

static bool acMappingsLoaded = false;
static jclass mcClass, playerClass, optionsClass, keyBindingClass;
static jfieldID instanceField, playerField, optionsField, attackKeyField;
static jfieldID yawField, pitchField;
static jmethodID setPressedMethod;

// PATCH 3 — persistent RNG for bleed randomisation
static std::mt19937 s_acRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

AutoClicker::AutoClicker() : Module("AutoClicker") {
    // init nextClickTime to now so first click fires after a proper random delay, not immediately
    nextClickTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() + 500LL; // small startup grace period
}

long long AutoClicker::GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int AutoClicker::GetRandomDelay() {
    float localMin = minCps;
    float localMax = maxCps;

    if (localMin < 0.1f) localMin = 0.1f;
    if (localMax < 0.1f) localMax = 0.1f;

    if (localMin >= localMax) {
        localMax = localMin + 1.0f;
    }

    int minDelay = (int)(1000.0f / localMax);
    int maxDelay = (int)(1000.0f / localMin);

    if (minDelay > maxDelay) {
        int temp = minDelay;
        minDelay = maxDelay;
        maxDelay = temp;
    }

    double mean   = (minDelay + maxDelay) / 2.0;
    double stddev = (maxDelay - minDelay) / 4.0;
    
    // FIX: Prevent std::normal_distribution crash when stddev is 0
    // This happens if minDelay == maxDelay (e.g. due to torn reads or very tight CPS bounds)
    if (stddev <= 0.001) stddev = 0.001; 
    
    std::normal_distribution<> distr(mean, stddev);

    int delay = (int)std::round(distr(s_acRng));

    if (delay < minDelay) delay = minDelay;
    if (delay > maxDelay) delay = maxDelay;

    // 3% fatigue spike
    std::uniform_int_distribution<> spikeDist(1, 100);
    if (spikeDist(s_acRng) <= 3) {
        delay += 50;
    }

    return delay;
}

void AutoClicker::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!acMappingsLoaded) {
        mcClass        = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        playerClass    = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;",  "net/minecraft/client/network/ClientPlayerEntity");
        optionsClass   = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;",  "net/minecraft/client/option/GameOptions");
        keyBindingClass= JNIHelper::FindClassSafe("Lnet/minecraft/class_304;",  "net/minecraft/client/option/KeyBinding");

        if (!mcClass || !playerClass || !optionsClass || !keyBindingClass) return;

        instanceField  = JNIHelper::GetStaticFieldSafe(mcClass,        "field_1700", "Lnet/minecraft/class_310;",  "instance");
        playerField    = JNIHelper::GetFieldSafe(mcClass,              "field_1724", "Lnet/minecraft/class_746;",  "player");
        optionsField   = JNIHelper::GetFieldSafe(mcClass,              "field_1690", "Lnet/minecraft/class_315;",  "options");
        attackKeyField = JNIHelper::GetFieldSafe(optionsClass,         "field_1886", "Lnet/minecraft/class_304;",  "attackKey");
        setPressedMethod = JNIHelper::GetMethodSafe(keyBindingClass,   "method_1430", "(Z)V",                      "setPressed");

        jclass entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        yawField   = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        acMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !optionsField || !attackKeyField) return;

    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        isClicking = false;
        // PATCH 3 — randomised bleed on LMB release
        std::uniform_real_distribution<float> bleedDist(0.5f, 0.7f);
        float bleedFactor = bleedDist(s_acRng);
        nextClickTime += (long long)(GetRandomDelay() * bleedFactor);
        return;
    }

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player  = env->GetObjectField(mc, playerField);
    jobject options = env->GetObjectField(mc, optionsField);

    if (!player || !options) {
        if (player)  env->DeleteLocalRef(player);
        if (options) env->DeleteLocalRef(options);
        env->DeleteLocalRef(mc);
        return;
    }

    long long currentTime = GetTimeMs();

    if (currentTime >= nextClickTime) {
        jobject attackKey = env->GetObjectField(options, attackKeyField);
        if (attackKey) {
            // Two-tick state machine for clicking.
            if (!isClicking) {
                // Tick 1: Press down
                env->CallVoidMethod(attackKey, setPressedMethod, JNI_TRUE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                isClicking = true;
                
                // Only apply jitter on the down-press
                if (jitter && yawField && pitchField) {
                    std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);

                    float currentYaw   = env->GetFloatField(player, yawField);
                    float currentPitch = env->GetFloatField(player, pitchField);

                    float newPitch = currentPitch + (jitterDist(s_acRng) * 0.5f);
                    if (newPitch >  90.0f) newPitch =  90.0f;
                    if (newPitch < -90.0f) newPitch = -90.0f;

                    env->SetFloatField(player, yawField,   currentYaw + jitterDist(s_acRng));
                    env->SetFloatField(player, pitchField, newPitch);
                }
                
                // Schedule the release for the very next tick (or ~10ms later)
                nextClickTime = currentTime + 10;
            } else {
                // Tick 2: Release
                env->CallVoidMethod(attackKey, setPressedMethod, JNI_FALSE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                isClicking = false;
                
                // Schedule the next down-press based on CPS delay
                // Subtract the 10ms we spent holding it down
                int delay = GetRandomDelay() - 10;
                if (delay < 1) delay = 1;
                nextClickTime = currentTime + delay;
            }

            env->DeleteLocalRef(attackKey);
        }
    }

    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
