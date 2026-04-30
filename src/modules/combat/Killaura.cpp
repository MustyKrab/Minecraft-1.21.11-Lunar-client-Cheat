#include "Killaura.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>

static bool mappingsLoaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, interactionManagerClass, handClass, listClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField;
static jfieldID entX, entY, entZ, mainHandField;
static jmethodID listSize, listGet, getHealth, attackMethod, swingMethod, getCooldownMethod;

Killaura::Killaura() : Module("Killaura"), reach(3.0f), fov(90.0f) {}

void Killaura::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!mappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        worldClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;", "net/minecraft/client/world/ClientWorld");
        entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        livingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        interactionManagerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;", "net/minecraft/client/network/ClientPlayerInteractionManager");
        handClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");
        listClass = env->FindClass("java/util/List");
        
        if (!mcClass || !worldClass || !entityClass || !livingClass || !playerClass || !interactionManagerClass || !handClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");
        
        playersField = JNIHelper::GetFieldSafe(worldClass, "field_18226", "Ljava/util/List;", "players");
        
        listSize = env->GetMethodID(listClass, "size", "()I");
        listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        // FOX FIX: Removed yaw and pitch field lookups since we aren't aiming anymore
        // We still need them locally to calculate FOV, but we can get them from the player
        jfieldID yawField = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        
        getHealth = JNIHelper::GetMethodSafe(livingClass, "method_6032", "()F", "getHealth");
        attackMethod = JNIHelper::GetMethodSafe(interactionManagerClass, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        swingMethod = JNIHelper::GetMethodSafe(livingClass, "method_6104", "(Lnet/minecraft/class_1268;)V", "swingHand");
        getCooldownMethod = JNIHelper::GetMethodSafe(playerClass, "method_7261", "(F)F", "getAttackCooldownProgress");

        mainHandField = JNIHelper::GetStaticFieldSafe(handClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");
        
        mappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField || !playersField || !entX || !entY || !entZ || !listSize || !listGet || !getHealth || !attackMethod || !swingMethod || !getCooldownMethod || !mainHandField) return;

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

    {
        double px = env->GetDoubleField(player, entX);
        double py = env->GetDoubleField(player, entY);
        double pz = env->GetDoubleField(player, entZ);
        
        jfieldID yawField = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        float currentYaw = yawField ? env->GetFloatField(player, yawField) : 0.0f;

        jobject playersList = env->GetObjectField(world, playersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, listSize);
        jobject bestTarget = nullptr;
        double bestDist = reach;
        float bestYawDiff = fov;

        for (int i = 0; i < size; i++) {
            jobject target = env->CallObjectMethod(playersList, listGet, i);
            if (!target) continue;

            if (env->IsSameObject(player, target)) {
                env->DeleteLocalRef(target);
                continue;
            }

            double tx = env->GetDoubleField(target, entX);
            double ty = env->GetDoubleField(target, entY);
            double tz = env->GetDoubleField(target, entZ);

            double dist = std::sqrt(std::pow(tx - px, 2) + std::pow(ty - py, 2) + std::pow(tz - pz, 2));
            
            if (dist <= bestDist) {
                float hp = env->CallFloatMethod(target, getHealth);
                if (hp > 0.0f) {
                    // FOV Check
                    double diffX = tx - px;
                    double diffZ = tz - pz;
                    float targetYaw = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
                    
                    float yawDiff = targetYaw - currentYaw;
                    while (yawDiff <= -180.0f) yawDiff += 360.0f;
                    while (yawDiff > 180.0f) yawDiff -= 360.0f;
                    
                    if (std::abs(yawDiff) <= fov) {
                        bestDist = dist;
                        if (bestTarget) env->DeleteLocalRef(bestTarget);
                        bestTarget = env->NewLocalRef(target);
                    }
                }
            }
            env->DeleteLocalRef(target);
        }

        if (bestTarget) {
            // FOX FIX: Removed all rotation/aimbot logic here. 
            // It now just attacks the target if it's within reach and FOV.

            // Attack (respect cooldown)
            float cooldown = env->CallFloatMethod(player, getCooldownMethod, 0.5f);
            if (cooldown >= 1.0f) {
                env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                
                jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                if (mainHand) {
                    env->CallVoidMethod(player, swingMethod, mainHand);
                    env->DeleteLocalRef(mainHand);
                }
            }

            env->DeleteLocalRef(bestTarget);
        }

        env->DeleteLocalRef(playersList);
    }

cleanup:
    env->DeleteLocalRef(interactionManager);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}