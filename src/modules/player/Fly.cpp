#include "Fly.h"
#include "../../core/JNIHelper.h"
#include <iostream>

static bool flyMappingsLoaded = false;
static jclass mcClass, playerClass, abilitiesClass;
static jfieldID instanceField, playerField, abilitiesField, flyingField;

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
        abilitiesField = JNIHelper::GetFieldSafe(playerClass, "field_7500", "Lnet/minecraft/class_1656;", "abilities");
        flyingField = JNIHelper::GetFieldSafe(abilitiesClass, "field_7478", "Z", "flying");

        flyMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !abilitiesField || !flyingField) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    if (player) {
        jobject abilities = env->GetObjectField(player, abilitiesField);
        if (abilities) {
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
        jobject abilities = env->GetObjectField(player, abilitiesField);
        if (abilities) {
            env->SetBooleanField(abilities, flyingField, JNI_FALSE);
            env->DeleteLocalRef(abilities);
        }
        env->DeleteLocalRef(player);
    }
    env->DeleteLocalRef(mc);
}
