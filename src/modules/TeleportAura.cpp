#include "TeleportAura.h"
#include "../core/JNIHelper.h"
#include <iostream>
#include <cmath>

static bool taMappingsLoaded = false;
static jclass taMcClass, taWorldClass, taEntityClass, taLivingClass, taPlayerClass, taInteractionManagerClass, taHandClass, taListClass;
static jclass taNetworkHandlerClass, taPositionAndOnGroundClass, taFullPacketClass;
static jfieldID taInstanceField, taPlayerField, taWorldField, taInteractionManagerField, taPlayersField, taNetworkHandlerField;
static jfieldID taEntX, taEntY, taEntZ, taYaw, taPitch, taMainHandField;
static jmethodID taListSize, taListGet, taGetHealth, taAttackMethod, taSwingMethod, taGetCooldownMethod, taSendPacketMethod, taPacketInitMethod, taFullPacketInitMethod;

TeleportAura::TeleportAura() : Module("TeleportAura"), reach(50.0f) {}

void TeleportAura::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!taMappingsLoaded) {
        taMcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        taWorldClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;", "net/minecraft/client/world/ClientWorld");
        taEntityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        taLivingClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        taPlayerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        taInteractionManagerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;", "net/minecraft/client/network/ClientPlayerInteractionManager");
        taHandClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");
        taListClass = env->FindClass("java/util/List");

        taNetworkHandlerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;", "net/minecraft/client/network/ClientPlayNetworkHandler");
        taPositionAndOnGroundClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2829;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$PositionAndOnGround");
        taFullPacketClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2830;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$Full");

        if (!taMcClass || !taWorldClass || !taEntityClass || !taLivingClass || !taPlayerClass ||
            !taInteractionManagerClass || !taHandClass || !taNetworkHandlerClass ||
            !taPositionAndOnGroundClass || !taFullPacketClass) return;

        taInstanceField = JNIHelper::GetStaticFieldSafe(taMcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        taPlayerField = JNIHelper::GetFieldSafe(taMcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        taWorldField = JNIHelper::GetFieldSafe(taMcClass, "field_1687", "Lnet/minecraft/class_638;", "world");
        taInteractionManagerField = JNIHelper::GetFieldSafe(taMcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");

        taNetworkHandlerField = JNIHelper::GetFieldSafe(
            JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity"),
            "field_3944", "Lnet/minecraft/class_634;", "networkHandler");

        taPlayersField = JNIHelper::GetFieldSafe(taWorldClass, "field_18226", "Ljava/util/List;", "players");

        taListSize = env->GetMethodID(taListClass, "size", "()I");
        taListGet  = env->GetMethodID(taListClass, "get",  "(I)Ljava/lang/Object;");

        taEntX  = JNIHelper::GetFieldSafe(taEntityClass, "field_6014", "D", "x");
        taEntY  = JNIHelper::GetFieldSafe(taEntityClass, "field_6036", "D", "y");
        taEntZ  = JNIHelper::GetFieldSafe(taEntityClass, "field_5969", "D", "z");
        taYaw   = JNIHelper::GetFieldSafe(taEntityClass, "field_5982", "F", "yaw");
        taPitch = JNIHelper::GetFieldSafe(taEntityClass, "field_5965", "F", "pitch");

        taGetHealth         = JNIHelper::GetMethodSafe(taLivingClass, "method_6032", "()F", "getHealth");
        taAttackMethod      = JNIHelper::GetMethodSafe(taInteractionManagerClass, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        taSwingMethod       = JNIHelper::GetMethodSafe(taLivingClass, "method_6104", "(Lnet/minecraft/class_1268;)V", "swingHand");
        taGetCooldownMethod = JNIHelper::GetMethodSafe(taPlayerClass, "method_7261", "(F)F", "getAttackCooldownProgress");
        taSendPacketMethod  = JNIHelper::GetMethodSafe(taNetworkHandlerClass, "method_52787", "(Lnet/minecraft/class_2596;)V", "sendPacket");

        taPacketInitMethod = env->GetMethodID(taPositionAndOnGroundClass, "<init>", "(DDDZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taPacketInitMethod = nullptr; }

        taFullPacketInitMethod = env->GetMethodID(taFullPacketClass, "<init>", "(DDDFFZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taFullPacketInitMethod = nullptr; }

        taMainHandField = JNIHelper::GetStaticFieldSafe(taHandClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");

        // Only mark loaded when ALL critical fields/methods resolved
        if (!taInstanceField || !taPlayerField || !taWorldField || !taInteractionManagerField ||
            !taPlayersField || !taEntX || !taEntY || !taEntZ || !taListSize || !taListGet ||
            !taGetHealth || !taAttackMethod || !taSwingMethod || !taGetCooldownMethod ||
            !taMainHandField || !taNetworkHandlerField || !taSendPacketMethod) return;

        if (!taPacketInitMethod && !taFullPacketInitMethod) return;

        taMappingsLoaded = true;
    }

    if (!taInstanceField || !taPlayerField || !taWorldField || !taInteractionManagerField ||
        !taPlayersField || !taEntX || !taEntY || !taEntZ || !taListSize || !taListGet ||
        !taGetHealth || !taAttackMethod || !taSwingMethod || !taGetCooldownMethod ||
        !taMainHandField || !taNetworkHandlerField) return;

    if (!taPacketInitMethod && !taFullPacketInitMethod) return;

    jobject mc = env->GetStaticObjectField(taMcClass, taInstanceField);
    if (!mc) return;

    jobject player             = env->GetObjectField(mc, taPlayerField);
    jobject world              = env->GetObjectField(mc, taWorldField);
    jobject interactionManager = env->GetObjectField(mc, taInteractionManagerField);

    if (!player || !world || !interactionManager) {
        if (player)             env->DeleteLocalRef(player);
        if (world)              env->DeleteLocalRef(world);
        if (interactionManager) env->DeleteLocalRef(interactionManager);
        env->DeleteLocalRef(mc);
        return;
    }

    {
        double px     = env->GetDoubleField(player, taEntX);
        double py     = env->GetDoubleField(player, taEntY);
        double pz     = env->GetDoubleField(player, taEntZ);
        float  pYaw   = env->GetFloatField(player, taYaw);
        float  pPitch = env->GetFloatField(player, taPitch);

        jobject playersList = env->GetObjectField(world, taPlayersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, taListSize);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(playersList);
            goto cleanup;
        }

        jobject bestTarget = nullptr;
        double  bestDist   = reach;

        for (int i = 0; i < size; i++) {
            jobject target = env->CallObjectMethod(playersList, taListGet, i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!target) continue;

            if (env->IsSameObject(player, target)) {
                env->DeleteLocalRef(target);
                continue;
            }

            double tx = env->GetDoubleField(target, taEntX);
            double ty = env->GetDoubleField(target, taEntY);
            double tz = env->GetDoubleField(target, taEntZ);

            double dist = std::sqrt(std::pow(tx - px, 2) + std::pow(ty - py, 2) + std::pow(tz - pz, 2));

            if (dist <= bestDist && dist > 3.0) {
                float hp = env->CallFloatMethod(target, taGetHealth);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
                if (hp > 0.0f) {
                    bestDist = dist;
                    if (bestTarget) env->DeleteLocalRef(bestTarget);
                    bestTarget = env->NewLocalRef(target);
                }
            }
            env->DeleteLocalRef(target);
        }

        if (bestTarget) {
            float cooldown = env->CallFloatMethod(player, taGetCooldownMethod, 0.5f);
            if (env->ExceptionCheck()) { env->ExceptionClear(); cooldown = 1.0f; }

            if (cooldown >= 1.0f) {
                double tx = env->GetDoubleField(bestTarget, taEntX);
                double ty = env->GetDoubleField(bestTarget, taEntY);
                double tz = env->GetDoubleField(bestTarget, taEntZ);

                double diffX = tx - px;
                double diffZ = tz - pz;

                jobject networkHandler = env->GetObjectField(player, taNetworkHandlerField);
                if (networkHandler) {
                    double tpX = tx - (diffX / bestDist) * 2.0;
                    double tpY = ty;
                    double tpZ = tz - (diffZ / bestDist) * 2.0;

                    jobject packetTo = nullptr;
                    if (taFullPacketInitMethod) {
                        packetTo = env->NewObject(taFullPacketClass, taFullPacketInitMethod, tpX, tpY, tpZ, pYaw, pPitch, JNI_TRUE);
                    } else if (taPacketInitMethod) {
                        packetTo = env->NewObject(taPositionAndOnGroundClass, taPacketInitMethod, tpX, tpY, tpZ, JNI_TRUE);
                    }
                    if (env->ExceptionCheck()) { env->ExceptionClear(); packetTo = nullptr; }

                    if (packetTo) {
                        env->CallVoidMethod(networkHandler, taSendPacketMethod, packetTo);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        env->DeleteLocalRef(packetTo);

                        env->CallVoidMethod(interactionManager, taAttackMethod, player, bestTarget);
                        if (env->ExceptionCheck()) env->ExceptionClear();

                        jobject mainHand = env->GetStaticObjectField(taHandClass, taMainHandField);
                        if (mainHand) {
                            env->CallVoidMethod(player, taSwingMethod, mainHand);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(mainHand);
                        }

                        jobject packetBack = nullptr;
                        if (taFullPacketInitMethod) {
                            packetBack = env->NewObject(taFullPacketClass, taFullPacketInitMethod, px, py, pz, pYaw, pPitch, JNI_TRUE);
                        } else if (taPacketInitMethod) {
                            packetBack = env->NewObject(taPositionAndOnGroundClass, taPacketInitMethod, px, py, pz, JNI_TRUE);
                        }
                        if (env->ExceptionCheck()) { env->ExceptionClear(); packetBack = nullptr; }

                        if (packetBack) {
                            env->CallVoidMethod(networkHandler, taSendPacketMethod, packetBack);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(packetBack);
                        }
                    }
                    env->DeleteLocalRef(networkHandler);
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
