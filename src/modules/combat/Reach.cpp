#include "Reach.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <windows.h>

static bool reachMappingsLoaded = false;
static jclass mcClass, playerClass, worldClass, interactionManagerClass, entityClass, livingClass, handClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField;
static jfieldID entX, entY, entZ, mainHandField;
static jmethodID listSize, listGet, attackMethod, swingMethod, getHealth;

Reach::Reach() : Module("Reach"), reachDistance(3.5f) {}

void Reach::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!reachMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        worldClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;", "net/minecraft/client/world/ClientWorld");
        interactionManagerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;", "net/minecraft/client/network/ClientPlayerInteractionManager");
        entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        livingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        handClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");

        if (!mcClass || !playerClass || !worldClass || !interactionManagerClass || !entityClass || !livingClass || !handClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");
        
        playersField = JNIHelper::GetFieldSafe(worldClass, "field_18226", "Ljava/util/List;", "players");
        
        jclass listClass = env->FindClass("java/util/List");
        listSize = env->GetMethodID(listClass, "size", "()I");
        listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        getHealth = JNIHelper::GetMethodSafe(livingClass, "method_6032", "()F", "getHealth");
        attackMethod = JNIHelper::GetMethodSafe(interactionManagerClass, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        swingMethod = JNIHelper::GetMethodSafe(livingClass, "method_6104", "(Lnet/minecraft/class_1268;)V", "swingHand");
        
        mainHandField = JNIHelper::GetStaticFieldSafe(handClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");

        reachMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField) return;

    static bool wasClicked = false;
    bool isClicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!isClicked || wasClicked) {
        wasClicked = isClicked;
        return;
    }
    wasClicked = true;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    jobject world = env->GetObjectField(mc, worldField);
    jobject interactionManager = env->GetObjectField(mc, interactionManagerField);

    if (!player || !world || !interactionManager) {
        if (player) env->DeleteLocalRef(player);
        if (world) env->DeleteLocalRef(world);
        if (interactionManager) env->DeleteLocalRef(interactionManager);
        env->DeleteLocalRef(mc);
        return;
    }

    double px = env->GetDoubleField(player, entX);
    double py = env->GetDoubleField(player, entY) + 1.62; 
    double pz = env->GetDoubleField(player, entZ);
    
    jfieldID yawField = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
    jfieldID pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");
    float yaw = env->GetFloatField(player, yawField);
    float pitch = env->GetFloatField(player, pitchField);

    float f = pitch * 0.017453292F;
    float g = -yaw * 0.017453292F;
    float h = std::cos(g);
    float i = std::sin(g);
    float j = std::cos(f);
    float k = std::sin(f);
    double lookX = (double)(i * j);
    double lookY = (double)(-k);
    double lookZ = (double)(h * j);

    jobject playersList = env->GetObjectField(world, playersField);
    if (!playersList) goto cleanup;

    int size = env->CallIntMethod(playersList, listSize);
    jobject bestTarget = nullptr;
    double bestDist = reachDistance;

    for (int idx = 0; idx < size; idx++) {
        jobject target = env->CallObjectMethod(playersList, listGet, idx);
        if (!target) continue;

        if (env->IsSameObject(player, target)) {
            env->DeleteLocalRef(target);
            continue;
        }

        float hp = env->CallFloatMethod(target, getHealth);
        if (hp <= 0.0f) {
            env->DeleteLocalRef(target);
            continue;
        }

        double tx = env->GetDoubleField(target, entX);
        double ty = env->GetDoubleField(target, entY) + 1.0; 
        double tz = env->GetDoubleField(target, entZ);

        double diffX = tx - px;
        double diffY = ty - py;
        double diffZ = tz - pz;
        double dist = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);

        if (dist <= bestDist && dist > 3.0) { 
            double dot = (diffX * lookX + diffY * lookY + diffZ * lookZ) / dist;
            if (dot > 0.95) { 
                bestDist = dist;
                if (bestTarget) env->DeleteLocalRef(bestTarget);
                bestTarget = env->NewLocalRef(target);
            }
        }
        env->DeleteLocalRef(target);
    }

    if (bestTarget) {
        env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
        jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
        env->CallVoidMethod(player, swingMethod, mainHand);
        env->DeleteLocalRef(mainHand);
        env->DeleteLocalRef(bestTarget);
    }

    env->DeleteLocalRef(playersList);

cleanup:
    env->DeleteLocalRef(interactionManager);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
