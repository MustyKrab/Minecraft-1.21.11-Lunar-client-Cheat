#include "Killaura.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>

static bool mappingsLoaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, interactionManagerClass, handClass, listClass;
static jclass networkHandlerClass, playerMoveC2SPacketClass, positionAndOnGroundClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField, networkHandlerField;
static jfieldID entX, entY, entZ, yawField, pitchField, mainHandField;
static jmethodID listSize, listGet, getHealth, attackMethod, swingMethod, getCooldownMethod, sendPacketMethod, packetInitMethod;

Killaura::Killaura() : Module("Killaura"), reach(4.5f), aimbotIntensity(0.5f), aimAssistMode(false), teleportAura(false) {}

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

        // Network classes for Teleport Aura
        networkHandlerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;", "net/minecraft/client/network/ClientPlayNetworkHandler");
        playerMoveC2SPacketClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket");
        positionAndOnGroundClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2829;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$PositionAndOnGround");

        if (!mcClass || !worldClass || !entityClass || !livingClass || !playerClass || !interactionManagerClass || !handClass || !networkHandlerClass || !playerMoveC2SPacketClass || !positionAndOnGroundClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");
        
        // We need networkHandler from player to send packets
        networkHandlerField = JNIHelper::GetFieldSafe(JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity"), "field_3944", "Lnet/minecraft/class_634;", "networkHandler");
        
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

        sendPacketMethod = JNIHelper::GetMethodSafe(networkHandlerClass, "method_52787", "(Lnet/minecraft/class_2596;)V", "sendPacket");
        packetInitMethod = env->GetMethodID(positionAndOnGroundClass, "<init>", "(DDDZ)V");

        mainHandField = JNIHelper::GetStaticFieldSafe(handClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");

        mappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField || !playersField || !entX || !entY || !entZ || !yawField || !pitchField || !listSize || !listGet || !getHealth || !attackMethod || !swingMethod || !getCooldownMethod || !mainHandField || !networkHandlerField || !sendPacketMethod || !packetInitMethod) return;

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

        jobject playersList = env->GetObjectField(world, playersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, listSize);
        jobject bestTarget = nullptr;
        double bestDist = reach;

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
                    bestDist = dist;
                    if (bestTarget) env->DeleteLocalRef(bestTarget);
                    bestTarget = env->NewLocalRef(target);
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

            float currentYaw = env->GetFloatField(player, yawField);
            float currentPitch = env->GetFloatField(player, pitchField);

            // Normalize yaw differences
            float yawDiff = targetYaw - currentYaw;
            while (yawDiff <= -180.0f) yawDiff += 360.0f;
            while (yawDiff > 180.0f) yawDiff -= 360.0f;

            float pitchDiff = targetPitch - currentPitch;

            // Apply smoothing based on intensity
            float smoothedYaw = currentYaw + (yawDiff * aimbotIntensity);
            float smoothedPitch = currentPitch + (pitchDiff * aimbotIntensity);

            env->SetFloatField(player, yawField, smoothedYaw);
            env->SetFloatField(player, pitchField, smoothedPitch);

            // Attack (respect cooldown)
            float cooldown = env->CallFloatMethod(player, getCooldownMethod, 0.5f);
            if (cooldown >= 1.0f) {
                
                // Teleport Aura Logic
                if (teleportAura && bestDist > 3.0) {
                    jobject networkHandler = env->GetObjectField(player, networkHandlerField);
                    if (networkHandler) {
                        // Calculate a point close to the target
                        double tpX = tx - (diffX / bestDist) * 2.0; // 2 blocks away from target
                        double tpY = ty;
                        double tpZ = tz - (diffZ / bestDist) * 2.0;

                        // 1. Send packet to teleport TO the target
                        jobject packetTo = env->NewObject(positionAndOnGroundClass, packetInitMethod, tpX, tpY, tpZ, JNI_TRUE);
                        env->CallVoidMethod(networkHandler, sendPacketMethod, packetTo);
                        env->DeleteLocalRef(packetTo);

                        // 2. Attack the target (now that we are "close" to it on the server)
                        env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                        
                        jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                        if (mainHand) {
                            env->CallVoidMethod(player, swingMethod, mainHand);
                            env->DeleteLocalRef(mainHand);
                        }

                        // 3. Send packet to teleport BACK to original position
                        jobject packetBack = env->NewObject(positionAndOnGroundClass, packetInitMethod, px, py, pz, JNI_TRUE);
                        env->CallVoidMethod(networkHandler, sendPacketMethod, packetBack);
                        env->DeleteLocalRef(packetBack);

                        env->DeleteLocalRef(networkHandler);
                    }
                } else {
                    // Normal Attack
                    env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                    
                    jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                    if (mainHand) {
                        env->CallVoidMethod(player, swingMethod, mainHand);
                        env->DeleteLocalRef(mainHand);
                    }
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
