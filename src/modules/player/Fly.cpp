#include "Fly.h"
#include "../../core/JNIHelper.h"
#include <iostream>

static bool flyMappingsLoaded = false;
static jclass mcClass, playerClass, abilitiesClass;
static jfieldID instanceField, playerField, flyingField, allowFlyingField;
static jmethodID getAbilitiesMethod;

Fly::Fly() : Module("Fly") {}

void Fly::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!flyMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        abilitiesClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1656;", "net/minecraft/entity/player/PlayerAbilities");

        if (!mcClass || !playerClass || !abilitiesClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        
        jclass playerEntityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        
        // FOX FIX: In newer versions, abilities might be accessed via a getter method rather than a direct field
        getAbilitiesMethod = JNIHelper::GetMethodSafe(playerEntityClass ? playerEntityClass : playerClass, "method_31549", "()Lnet/minecraft/class_1656;", "getAbilities");
        
        flyingField = JNIHelper::GetFieldSafe(abilitiesClass, "field_7478", "Z", "flying");
        allowFlyingField = JNIHelper::GetFieldSafe(abilitiesClass, "field_7479", "Z", "allowFlying");

        flyMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !getAbilitiesMethod || !flyingField || !allowFlyingField) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    if (player) {
        jobject abilities = env->CallObjectMethod(player, getAbilitiesMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            abilities = nullptr;
        }

        if (abilities) {
            env->SetBooleanField(abilities, allowFlyingField, JNI_TRUE);
            env->SetBooleanField(abilities, flyingField, JNI_TRUE);
            env->DeleteLocalRef(abilities);
        }
        env->DeleteLocalRef(player);
    }
    env->DeleteLocalRef(mc);
}

void Fly::OnDisable() {
    JNIEnv* env = JNIHelper::env;
    if (!env || !flyMappingsLoaded) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    if (player) {
        jobject abilities = env->CallObjectMethod(player, getAbilitiesMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            abilities = nullptr;
        }

        if (abilities) {
            // FOX FIX: Only disable flying, don't disable allowFlying if they are in creative mode
            // For a simple cheat, just setting flying to false is enough to drop them
            env->SetBooleanField(abilities, flyingField, JNI_FALSE);
            env->DeleteLocalRef(abilities);
        }
        env->DeleteLocalRef(player);
    }
    env->DeleteLocalRef(mc);
}
