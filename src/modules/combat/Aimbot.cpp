#include "Aimbot.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <windows.h>
#include <random>

static bool aimbotMappingsLoaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, optionsClass, doubleOptionClass;
static jfieldID instanceField, playerField, worldField, playersField, optionsField, sensitivityField;
static jfieldID entX, entY, entZ, yawField, pitchField;
static jmethodID listSize, listGet, getHealth, getDoubleValue;

Aimbot::Aimbot() : Module("Aimbot"), fov(90.0f), smoothSpeed(0.15f) {}

void Aimbot::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!aimbotMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        worldClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;", "net/minecraft/client/world/ClientWorld");
        entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        livingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        playerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        optionsClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;", "net/minecraft/client/option/GameOptions");
        doubleOptionClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_7172;", "net/minecraft/client/option/SimpleOption");
        
        jclass listClass = env->FindClass("java/util/List");

        if (!mcClass || !worldClass || !entityClass || !livingClass || !playerClass || !optionsClass || !doubleOptionClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
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
        
        sensitivityField = JNIHelper::GetFieldSafe(optionsClass, "field_1844", "Lnet/minecraft/class_7172;", "mouseSensitivity");
        getDoubleValue = JNIHelper::GetMethodSafe(doubleOptionClass, "method_41753", "()Ljava/lang/Object;", "getValue");

        aimbotMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !optionsField || !playersField || !entX || !entY || !entZ || !yawField || !pitchField || !listSize || !listGet || !getHealth) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    // Only aim if left mouse button is held (Aim Assist style)
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        env->DeleteLocalRef(mc);
        return;
    }

    jobject player = env->GetObjectField(mc, playerField);
    jobject world = env->GetObjectField(mc, worldField);
    jobject options = env->GetObjectField(mc, optionsField);

    if (!player || !world || !options) {
        if (player) env->DeleteLocalRef(player);
        if (world) env->DeleteLocalRef(world);
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
        double bestDist = 6.0; // Max aim assist range
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
                    double diffX = tx - px;
                    double diffZ = tz - pz;
                    float targetYaw = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
                    
                    float yawDiff = targetYaw - currentYaw;
                    while (yawDiff <= -180.0f) yawDiff += 360.0f;
                    while (yawDiff > 180.0f) yawDiff -= 360.0f;
                    
                    if (std::abs(yawDiff) < bestYawDiff) {
                        bestYawDiff = std::abs(yawDiff);
                        if (bestTarget) env->DeleteLocalRef(bestTarget);
                        bestTarget = env->NewLocalRef(target);
                    }
                }
            }
            env->DeleteLocalRef(target);
        }

        if (bestTarget) {
            double tx = env->GetDoubleField(bestTarget, entX);
            double ty = env->GetDoubleField(bestTarget, entY);
            double tz = env->GetDoubleField(bestTarget, entZ);

            double diffX = tx - px;
            double diffY = (ty + 1.0) - (py + 1.62); // Aim at chest
            double diffZ = tz - pz;
            double distXZ = std::sqrt(diffX * diffX + diffZ * diffZ);

            float targetYaw = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
            float targetPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);

            // Smooth interpolation (slerp)
            float yawDiff = targetYaw - currentYaw;
            while (yawDiff <= -180.0f) yawDiff += 360.0f;
            while (yawDiff > 180.0f) yawDiff -= 360.0f;

            float pitchDiff = targetPitch - currentPitch;
            
            // FOX FIX: Add randomization to the smoothing to bypass heuristic checks
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> randomSmooth(smoothSpeed * 0.8f, smoothSpeed * 1.2f);
            float actualSmoothSpeed = randomSmooth(gen);
            
            float newYaw = currentYaw + (yawDiff * actualSmoothSpeed);
            float newPitch = currentPitch + (pitchDiff * actualSmoothSpeed);

            // Apply GCD (Greatest Common Divisor) fix to bypass server-side rotation checks
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

                        newYaw -= std::fmod(newYaw, gcd);
                        newPitch -= std::fmod(newPitch, gcd);
                        
                        // FOX FIX: Delete local ref to prevent table overflow crash
                        env->DeleteLocalRef(doubleClass);
                        env->DeleteLocalRef(sensDoubleObj);
                    }
                    env->DeleteLocalRef(sensObj);
                }
            }

            env->SetFloatField(player, yawField, newYaw);
            env->SetFloatField(player, pitchField, newPitch);

            env->DeleteLocalRef(bestTarget);
        }

        env->DeleteLocalRef(playersList);
    }

cleanup:
    env->DeleteLocalRef(options);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}