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
static jmethodID getKeyCodeMethod;

// PATCH 5 — stochastic tick interval
static DWORD s_wtapNextTickMs = 0;

static std::mt19937 s_wtapRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

static float gaussian_noise_wtap(float stddev) {
    std::uniform_real_distribution<float> u(1e-6f, 1.0f);
    float u1 = u(s_wtapRng), u2 = u(s_wtapRng);
    return stddev * std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530f * u2);
}

WTap::WTap() : Module("WTap") {}

void WTap::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!wtapMappingsLoaded) {
        retryCounter++;
        if (retryCounter < 60) return;
        retryCounter = 0;

        mcClass        = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        playerClass    = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;",  "net/minecraft/client/network/ClientPlayerEntity");
        optionsClass   = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;",  "net/minecraft/client/option/GameOptions");
        keyBindingClass= JNIHelper::FindClassSafe("Lnet/minecraft/class_304;",  "net/minecraft/client/option/KeyBinding");

        if (!mcClass || !playerClass || !optionsClass || !keyBindingClass) return;

        instanceField       = JNIHelper::GetStaticFieldSafe(mcClass,       "field_1700",  "Lnet/minecraft/class_310;",  "instance");
        playerField         = JNIHelper::GetFieldSafe(mcClass,             "field_1724",  "Lnet/minecraft/class_746;",  "player");
        optionsField        = JNIHelper::GetFieldSafe(mcClass,             "field_1690",  "Lnet/minecraft/class_315;",  "options");
        forwardKeyField     = JNIHelper::GetFieldSafe(optionsClass,        "field_1894",  "Lnet/minecraft/class_304;",  "forwardKey");

        swingProgressField  = JNIHelper::GetFieldSafe(playerClass,         "field_6220",  "I",                          "handSwingTicks");

        setPressedMethod    = JNIHelper::GetMethodSafe(keyBindingClass,    "method_1430", "(Z)V",                       "setPressed");
        setSprintingMethod  = JNIHelper::GetMethodSafe(playerClass,        "method_5728", "(Z)V",                       "setSprinting");

        getKeyCodeMethod    = JNIHelper::GetMethodSafe(keyBindingClass,    "method_1428", "()I",                        "getBoundKeyCode");

        wtapMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !optionsField || !swingProgressField) return;

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

    int swingTicks = env->GetIntField(player, swingProgressField);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        swingTicks = 0;
    }

    if (swingTicks == 1 || swingTicks == 2) {
        // FIX: Only trigger WTap if the player is actually sprinting
        // If they aren't sprinting, un-sprinting and re-sprinting does nothing for knockback
        jmethodID isSprintingMethod = JNIHelper::GetMethodSafe(playerClass, "method_5624", "()Z", "isSprinting");
        bool isSprinting = false;
        
        if (isSprintingMethod) {
            isSprinting = env->CallBooleanMethod(player, isSprintingMethod) == JNI_TRUE;
            if (env->ExceptionCheck()) env->ExceptionClear();
        } else {
            // Fallback: assume they are sprinting if W is held
            isSprinting = (GetAsyncKeyState('W') & 0x8000) != 0;
        }

        if (isSprinting) {
            isWTapping = true;
            ticksSinceAttack = 0;

            std::uniform_int_distribution<> delayDist(2, 6);
            wTapDelay = delayDist(s_wtapRng);

            env->CallVoidMethod(player, setSprintingMethod, JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();

            jobject forwardKey = env->GetObjectField(options, forwardKeyField);
            if (forwardKey) {
                env->CallVoidMethod(forwardKey, setPressedMethod, JNI_FALSE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(forwardKey);
            }
        }
    }

    if (isWTapping) {
        // PATCH 5 — stochastic tick interval gate (only applied to the re-sprint restore path)
        {
            DWORD now = (DWORD)GetTickCount64();
            if (now < s_wtapNextTickMs) goto done;
            std::uniform_int_distribution<int> baseDist(35, 60);
            int base   = baseDist(s_wtapRng);
            int jitter = (int)gaussian_noise_wtap(4.0f);
            s_wtapNextTickMs = now + (DWORD)(base + jitter);
        }

        ticksSinceAttack++;
        if (ticksSinceAttack >= wTapDelay) {
            isWTapping = false;

            bool wHeld = false;
            if (getKeyCodeMethod && forwardKeyField) {
                jobject forwardKey = env->GetObjectField(options, forwardKeyField);
                if (forwardKey) {
                    int keyCode = env->CallIntMethod(forwardKey, getKeyCodeMethod);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); keyCode = 87; }
                    wHeld = (GetAsyncKeyState('W') & 0x8000) != 0;
                    env->DeleteLocalRef(forwardKey);
                }
            } else {
                wHeld = (GetAsyncKeyState('W') & 0x8000) != 0;
            }

            if (wHeld) {
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

done:
    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
