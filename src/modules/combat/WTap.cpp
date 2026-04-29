#include "WTap.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <windows.h>
#include <random>

static bool wtapMappingsLoaded = false;
static jclass mcClass, playerClass, optionsClass, keyBindingClass;
static jfieldID instanceField, playerField, optionsField, forwardKeyField;
static jmethodID setPressedMethod, setSprintingMethod, isAttackingMethod;

WTap::WTap() : Module("WTap") {}

void WTap::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!wtapMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        optionsClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;", "net/minecraft/client/option/GameOptions");
        keyBindingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_304;", "net/minecraft/client/option/KeyBinding");

        if (!mcClass || !playerClass || !optionsClass || !keyBindingClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        optionsField = JNIHelper::GetFieldSafe(mcClass, "field_1690", "Lnet/minecraft/class_315;", "options");
        
        forwardKeyField = JNIHelper::GetFieldSafe(optionsClass, "field_1894", "Lnet/minecraft/class_304;", "forwardKey");
        
        setPressedMethod = JNIHelper::GetMethodSafe(keyBindingClass, "method_1430", "(Z)V", "setPressed");
        setSprintingMethod = JNIHelper::GetMethodSafe(playerClass, "method_5728", "(Z)V", "setSprinting");
        
        wtapMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !optionsField) return;

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

    // Check if player just swung their hand (approximate attack detection for external)
    jfieldID swingProgressField = JNIHelper::GetFieldSafe(playerClass, "field_6220", "I", "handSwingTicks");
    if (swingProgressField) {
        int swingTicks = env->GetIntField(player, swingProgressField);
        
        // If swing just started (tick 1 or 2) and we are holding W
        if (swingTicks == 1 || swingTicks == 2) {
            // Start W-Tap
            isWTapping = true;
            ticksSinceAttack = 0;
            
            // FOX FIX: Add randomization to the W-Tap delay
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> delayDist(2, 6); // Randomize between 2 and 6 ticks
            wTapDelay = delayDist(gen);
            
            // Un-sprint
            env->CallVoidMethod(player, setSprintingMethod, JNI_FALSE);
            
            jobject forwardKey = env->GetObjectField(options, forwardKeyField);
            if (forwardKey) {
                env->CallVoidMethod(forwardKey, setPressedMethod, JNI_FALSE);
                env->DeleteLocalRef(forwardKey);
            }
        }
    }

    if (isWTapping) {
        ticksSinceAttack++;
        if (ticksSinceAttack >= wTapDelay) {
            // End W-Tap (re-sprint)
            isWTapping = false;
            
            // Only press W again if the user is actually holding the physical W key
            if (GetAsyncKeyState('W') & 0x8000) {
                jobject forwardKey = env->GetObjectField(options, forwardKeyField);
                if (forwardKey) {
                    env->CallVoidMethod(forwardKey, setPressedMethod, JNI_TRUE);
                    env->DeleteLocalRef(forwardKey);
                }
                env->CallVoidMethod(player, setSprintingMethod, JNI_TRUE);
            }
        }
    }

    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
