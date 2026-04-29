#include "AutoClicker.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <chrono>
#include <random>
#include <windows.h>

static bool acMappingsLoaded = false;
static jclass mcClass, playerClass, optionsClass, keyBindingClass;
static jfieldID instanceField, playerField, optionsField, attackKeyField;
static jfieldID yawField, pitchField;
static jmethodID setPressedMethod;

AutoClicker::AutoClicker() : Module("AutoClicker") {}

long long AutoClicker::GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int AutoClicker::GetRandomDelay() {
    if (minCps >= maxCps) maxCps = minCps + 1.0f;
    
    // Convert CPS to milliseconds delay
    // 10 CPS = 100ms per click (50ms down, 50ms up)
    int minDelay = (int)(1000.0f / maxCps);
    int maxDelay = (int)(1000.0f / minCps);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(minDelay, maxDelay);
    
    return distr(gen);
}

void AutoClicker::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!acMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        optionsClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;", "net/minecraft/client/option/GameOptions");
        keyBindingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_304;", "net/minecraft/client/option/KeyBinding");

        if (!mcClass || !playerClass || !optionsClass || !keyBindingClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        optionsField = JNIHelper::GetFieldSafe(mcClass, "field_1690", "Lnet/minecraft/class_315;", "options");
        
        attackKeyField = JNIHelper::GetFieldSafe(optionsClass, "field_1886", "Lnet/minecraft/class_304;", "attackKey");
        
        setPressedMethod = JNIHelper::GetMethodSafe(keyBindingClass, "method_1430", "(Z)V", "setPressed");
        
        jclass entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        yawField = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        acMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !optionsField || !attackKeyField) return;

    // Only autoclick if the user is physically holding the left mouse button
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        isClicking = false;
        return;
    }

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    jobject options = env->GetObjectField(mc, optionsField);

    if (!player || !options) {
        if (player) env->DeleteLocalRef(player);
        if (options) env->DeleteLocalRef(options);
        env->DeleteLocalRef(mc);
        return;
    }

    long long currentTime = GetTimeMs();

    if (currentTime >= nextClickTime) {
        jobject attackKey = env->GetObjectField(options, attackKeyField);
        if (attackKey) {
            // Toggle click state
            isClicking = !isClicking;
            env->CallVoidMethod(attackKey, setPressedMethod, isClicking ? JNI_TRUE : JNI_FALSE);
            
            // If we just clicked down, apply jitter
            if (isClicking && jitter) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> jitterDist(-0.5f, 0.5f);
                
                float currentYaw = env->GetFloatField(player, yawField);
                float currentPitch = env->GetFloatField(player, pitchField);
                
                env->SetFloatField(player, yawField, currentYaw + jitterDist(gen));
                env->SetFloatField(player, pitchField, currentPitch + (jitterDist(gen) * 0.5f));
            }
            
            env->DeleteLocalRef(attackKey);
            
            // Set next time to toggle state (half of the full delay to simulate down/up)
            nextClickTime = currentTime + (GetRandomDelay() / 2);
        }
    }

    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
