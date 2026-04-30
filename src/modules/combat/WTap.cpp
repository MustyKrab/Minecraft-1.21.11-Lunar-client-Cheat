#include "WTap.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <windows.h>
#include <random>

static bool wtapMappingsLoaded = false;
static int retryCounter = 0;
static jclass mcClass, playerClass, optionsClass, keyBindingClass;
static jfieldID instanceField, playerField, optionsField, forwardKeyField, swingProgressField;
static jmethodID setPressedMethod, setSprintingMethod;

WTap::WTap() : Module("WTap") {}

void WTap::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!wtapMappingsLoaded) {
        retryCounter++;
        if (retryCounter < 60) return;
        retryCounter = 0;

        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        optionsClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;", "net/minecraft/client/option/GameOptions");
        keyBindingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_304;", "net/minecraft/client/option/KeyBinding");

        if (!mcClass || !playerClass || !optionsClass || !keyBindingClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        optionsField = JNIHelper::GetFieldSafe(mcClass, "field_1690", "Lnet/minecraft/class_315;", "options");
        
        forwardKeyField = JNIHelper::GetFieldSafe(optionsClass, "field_1894", "Lnet/minecraft/class_304;", "forwardKey");
        swingProgressField = JNIHelper::GetFieldSafe(playerClass, "field_6220", "I", "handSwingTicks");
        
        setPressedMethod = JNIHelper::GetMethodSafe(keyBindingClass, "method_1430", "(Z)V", "setPressed");
        setSprintingMethod = JNIHelper::GetMethodSafe(playerClass, "method_5728", "(Z)V", "setSprinting");
        
        wtapMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !optionsField || !swingProgressField) return;

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

    int swingTicks = env->GetIntField(player, swingProgressField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        swingTicks = 0;
    }
    
    if (swingTicks == 1 || swingTicks == 2) {
        isWTapping = true;
        ticksSinceAttack = 0;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delayDist(2, 6); 
        wTapDelay = delayDist(gen);
        
        env->CallVoidMethod(player, setSprintingMethod, JNI_FALSE);
        if (env->ExceptionCheck()) env->ExceptionClear();
        
        jobject forwardKey = env->GetObjectField(options, forwardKeyField);
        if (forwardKey) {
            env->CallVoidMethod(forwardKey, setPressedMethod, JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(forwardKey);
        }
    }

    if (isWTapping) {
        ticksSinceAttack++;
        if (ticksSinceAttack >= wTapDelay) {
            isWTapping = false;
            
            if (GetAsyncKeyState('W') & 0x8000) {
                jobject forwardKey = env->GetObjectField(options, forwardKeyField);
                if (forwardKey) {
                    env->CallVoidMethod(forwardKey, setPressedMethod, JNI_TRUE);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(forwardKey);
                }
                env->CallVoidMethod(player, setSprintingMethod, JNI_TRUE);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }

    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}