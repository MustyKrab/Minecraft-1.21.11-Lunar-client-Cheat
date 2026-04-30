#include "Killaura.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <random>

static bool mappingsLoaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, interactionManagerClass, handClass, listClass;
static jclass optionsClass, doubleOptionClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField;
static jfieldID entX, entY, entZ, yawField, pitchField, mainHandField, optionsField, sensitivityField;
static jmethodID listSize, listGet, getHealth, attackMethod, swingMethod, getCooldownMethod, getDoubleValue;

Killaura::Killaura() : Module("Killaura"), reach(3.0f), aimbotIntensity(0.5f), aimAssistMode(false), fov(90.0f) {}

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
        
        optionsClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;", "net/minecraft/client/option/GameOptions");
        doubleOptionClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_7172;", "net/minecraft/client/option/SimpleOption");

        if (!mcClass || !worldClass || !entityClass || !livingClass || !playerClass || !interactionManagerClass || !handClass || !optionsClass || !doubleOptionClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");
        optionsField = JNIHelper::GetFieldSafe(mcClass, "field_1690", "Lnet/minecraft/class_315;", "options");
        
        playersField = JNIHelper::GetFieldSafe(worldClass, "field_18226", "Ljava/util/List;", "players");
        
        listSize = env->GetMethodID(listClass, "size", "()I");
        listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");
        yawField = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        getHealth = JNIHelper::GetMethodSafe(livingClass, "method_6032", "()F", "getHealth");
        attackMethod = JNIHelper::GetMethodSafe(interactionManagerClass, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        swingMethod = JNIHelper::GetMethodSafe(livingClass, "method_6104", "(Lnet/minecraft/class_1268;)V", "swingHand");
        getCooldownMethod = JNIHelper::GetMethodSafe(playerClass, "method_7261", "(F)F", "getAttackCooldownProgress");

        mainHandField = JNIHelper::GetStaticFieldSafe(handClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");
        
        sensitivityField = JNIHelper::GetFieldSafe(optionsClass, "field_1844", "Lnet/minecraft/class_7172;", "mouseSensitivity");
        getDoubleValue = JNIHelper::GetMethodSafe(doubleOptionClass, "method_41753", "()Ljava/lang/Object;", "getValue");

        mappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField || !playersField || !entX || !entY || !entZ || !yawField || !pitchField || !listSize || !listGet || !getHealth || !attackMethod || !swingMethod || !getCooldownMethod || !mainHandField || !optionsField) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    jobject world = env->GetObjectField(mc, worldField);
    jobject interactionManager = env->GetObjectField(mc, interactionManagerField);
    jobject options = env->GetObjectField(mc, optionsField);

    if (!player || !world || !interactionManager || !options) {
        if (player) env->DeleteLocalRef(player);
        if (world) env->DeleteLocalRef(world);
        if (interactionManager) env->DeleteLocalRef(interactionManager);
        if (options) env->DeleteLocalRef(options);
        env->DeleteLocalRef(mc);
        return;
    }

    {
        double px = env->GetDoubleField(player, entX);
        double py = env->GetDoubleField(player, entY);
        double pz = env->GetDoubleField(player, entZ);
        float currentYaw = env->GetFloatField(player, yawField);
        float currentPitch = env->GetFloatField(player, pitchField);

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
                    // FOX FIX: Check FOV to prevent attacking entities behind us (Sentinel flag)
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
            // Rotations
            double tx = env->GetDoubleField(bestTarget, entX);
            double ty = env->GetDoubleField(bestTarget, entY);
            double tz = env->GetDoubleField(bestTarget, entZ);

            double diffX = tx - px;
            double diffY = (ty + 1.0) - (py + 1.62); // Aim at chest
            double diffZ = tz - pz;
            double distXZ = std::sqrt(diffX * diffX + diffZ * diffZ);

            float targetYaw = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
            float targetPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);

            // Normalize yaw differences
            float yawDiff = targetYaw - currentYaw;
            while (yawDiff <= -180.0f) yawDiff += 360.0f;
            while (yawDiff > 180.0f) yawDiff -= 360.0f;

            float pitchDiff = targetPitch - currentPitch;

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> randomSmooth(aimbotIntensity * 0.8f, aimbotIntensity * 1.2f);
            float actualIntensity = randomSmooth(gen);

            float smoothedYaw = currentYaw + (yawDiff * actualIntensity);
            float smoothedPitch = currentPitch + (pitchDiff * actualIntensity);

            // FOX FIX: Apply GCD (Greatest Common Divisor) fix to bypass Sentinel rotation checks
            if (sensitivityField && getDoubleValue) {
                jobject sensObj = env->GetObjectField(options, sensitivityField);
                if (sensObj) {
                    jobject sensDoubleObj = env->CallObjectMethod(sensObj, getDoubleValue);
                    if (sensDoubleObj) {
                        jclass doubleClass = env->FindClass("java/lang/Double");
                        jmethodID doubleValueMethod = env->GetMethodID(doubleClass, "doubleValue", "()D");
                        double sens = env->CallDoubleMethod(sensDoubleObj, doubleValueMethod);
                        
                        float f = (float)(sens * 0.6 + 0.2);
                        float gcd = f * f * f * 1.2f;

                        smoothedYaw -= std::fmod(smoothedYaw, gcd);
                        smoothedPitch -= std::fmod(smoothedPitch, gcd);
                        
                        env->DeleteLocalRef(doubleClass);
                        env->DeleteLocalRef(sensDoubleObj);
                    }
                    env->DeleteLocalRef(sensObj);
                }
            }

            env->SetFloatField(player, yawField, smoothedYaw);
            env->SetFloatField(player, pitchField, smoothedPitch);

            // Attack (respect cooldown)
            float cooldown = env->CallFloatMethod(player, getCooldownMethod, 0.5f);
            if (cooldown >= 1.0f) {
                // Normal Attack
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
    env->DeleteLocalRef(options);
    env->DeleteLocalRef(interactionManager);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}