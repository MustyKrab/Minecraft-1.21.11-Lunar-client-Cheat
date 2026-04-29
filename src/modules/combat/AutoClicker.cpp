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

AutoClicker::AutoClicker() : Module("AutoClicker") {}

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
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // FOX FIX: Use a normal distribution instead of uniform for more human-like clicking
    // Mean is the average delay, stddev is the variance
    double mean = (minDelay + maxDelay) / 2.0;
    double stddev = (maxDelay - minDelay) / 4.0; 
    std::normal_distribution<> distr(mean, stddev);
    
    int delay = (int)std::round(distr(gen));
    
    // Clamp the delay to our min/max bounds
    if (delay < minDelay) delay = minDelay;
    if (delay > maxDelay) delay = maxDelay;
    
    // Add occasional random spikes (drops in CPS) to simulate human fatigue
    std::uniform_int_distribution<> spikeDistr(1, 100);
    if (spikeDistr(gen) <= 3) { // 3% chance of a spike
        delay += 50; // Add 50ms to simulate a missed click or hesitation
    }
    
    return delay;
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
            isClicking = !isClicking;
            env->CallVoidMethod(attackKey, setPressedMethod, isClicking ? JNI_TRUE : JNI_FALSE);
            
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
            
            nextClickTime = currentTime + (GetRandomDelay() / 2);
        }
    }

    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
