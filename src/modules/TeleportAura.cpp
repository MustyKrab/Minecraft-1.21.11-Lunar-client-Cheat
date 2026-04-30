#include "TeleportAura.h"
#include "../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <random>

static bool taMappingsLoaded = false;
static jclass taMcClass, taWorldClass, taEntityClass, taLivingClass, taPlayerClass, taInteractionManagerClass, taHandClass, taListClass;
static jclass taNetworkHandlerClass, taPositionAndOnGroundClass, taFullPacketClass;
static jfieldID taInstanceField, taPlayerField, taWorldField, taInteractionManagerField, taPlayersField, taNetworkHandlerField;
static jfieldID taEntX, taEntY, taEntZ, taYaw, taPitch, taMainHandField;
static jmethodID taListSize, taListGet, taGetHealth, taAttackMethod, taSwingMethod, taGetCooldownMethod, taSendPacketMethod, taPacketInitMethod, taFullPacketInitMethod;

// PATCH 1 – rotation velocity caps for TeleportAura rotation writes
static constexpr float kTAMaxYawPerTick   = 18.0f;
static constexpr float kTAMaxPitchPerTick = 12.0f;

// PATCH 4 – target-switch cooldown state
static void*  s_taLastTargetId    = nullptr;
static DWORD  s_taTargetLostMs    = 0;
static bool   s_taTargetWasActive = false;

// PATCH 5 – stochastic tick interval
static DWORD  s_taNextTickMs = 0;

static std::mt19937 s_taRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

static float gaussian_noise_ta(float stddev) {
    std::uniform_real_distribution<float> u(1e-6f, 1.0f);
    float u1 = u(s_taRng), u2 = u(s_taRng);
    return stddev * std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530f * u2);
}

TeleportAura::TeleportAura() : Module("TeleportAura"), reach(50.0f) {}

void TeleportAura::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!taMappingsLoaded) {
        taMcClass                  = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        taWorldClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",  "net/minecraft/client/world/ClientWorld");
        taEntityClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        taLivingClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        taPlayerClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        taInteractionManagerClass  = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;",  "net/minecraft/client/network/ClientPlayerInteractionManager");
        taHandClass                = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");
        taListClass                = env->FindClass("java/util/List");

        taNetworkHandlerClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;",  "net/minecraft/client/network/ClientPlayNetworkHandler");
        taPositionAndOnGroundClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2829;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$PositionAndOnGround");
        taFullPacketClass          = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2830;", "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$Full");

        if (!taMcClass || !taWorldClass || !taEntityClass || !taLivingClass || !taPlayerClass ||
            !taInteractionManagerClass || !taHandClass || !taNetworkHandlerClass ||
            !taPositionAndOnGroundClass || !taFullPacketClass) return;

        taInstanceField           = JNIHelper::GetStaticFieldSafe(taMcClass,   "field_1700",  "Lnet/minecraft/class_310;", "instance");
        taPlayerField             = JNIHelper::GetFieldSafe(taMcClass,         "field_1724",  "Lnet/minecraft/class_746;", "player");
        taWorldField              = JNIHelper::GetFieldSafe(taMcClass,         "field_1687",  "Lnet/minecraft/class_638;", "world");
        taInteractionManagerField = JNIHelper::GetFieldSafe(taMcClass,         "field_1761",  "Lnet/minecraft/class_636;", "interactionManager");

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

        taGetHealth        = JNIHelper::GetMethodSafe(taLivingClass,           "method_6032",  "()F",  "getHealth");
        taAttackMethod     = JNIHelper::GetMethodSafe(taInteractionManagerClass,"method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        taSwingMethod      = JNIHelper::GetMethodSafe(taLivingClass,           "method_6104",  "(Lnet/minecraft/class_1268;)V", "swingHand");
        taGetCooldownMethod= JNIHelper::GetMethodSafe(taPlayerClass,           "method_7261",  "(F)F",  "getAttackCooldownProgress");
        taSendPacketMethod = JNIHelper::GetMethodSafe(taNetworkHandlerClass,   "method_52787", "(Lnet/minecraft/class_2596;)V", "sendPacket");

        taPacketInitMethod = env->GetMethodID(taPositionAndOnGroundClass, "<init>", "(DDDZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taPacketInitMethod = nullptr; }

        taFullPacketInitMethod = env->GetMethodID(taFullPacketClass, "<init>", "(DDDFZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taFullPacketInitMethod = nullptr; }

        taMainHandField = JNIHelper::GetStaticFieldSafe(taHandClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");

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

    // PATCH 5 – stochastic tick interval
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_taNextTickMs) return;
        std::uniform_int_distribution<int> baseDist(35, 60);
        int base = baseDist(s_taRng);
        int jitter = (int)gaussian_noise_ta(4.0f);
        s_taNextTickMs = now + (DWORD)(base + jitter);
    }

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
        double px    = env->GetDoubleField(player, taEntX);
        double py    = env->GetDoubleField(player, taEntY);
        double pz    = env->GetDoubleField(player, taEntZ);
        float  pYaw  = env->GetFloatField(player, taYaw);
        float  pPitch= env->GetFloatField(player, taPitch);

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
        void*   bestTargetId = nullptr;

        // PATCH 2 – randomised aim-height jitter
        std::uniform_real_distribution<float> heightJitter(0.6f, 1.4f);
        float aimHeight = heightJitter(s_taRng);

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
                    bestTargetId = (void*)target;
                    if (bestTarget) env->DeleteLocalRef(bestTarget);
                    bestTarget = env->NewLocalRef(target);
                }
            }
            env->DeleteLocalRef(target);
        }

        // PATCH 4 – target-switch cooldown
        DWORD nowMs = (DWORD)GetTickCount64();
        if (bestTargetId != s_taLastTargetId) {
            if (s_taTargetWasActive) s_taTargetLostMs = nowMs;
            s_taLastTargetId      = bestTargetId;
            s_taTargetWasActive   = (bestTarget != nullptr);
            if (bestTarget) {
                std::uniform_int_distribution<int> cdDist(150, 300);
                if (nowMs - s_taTargetLostMs < (DWORD)cdDist(s_taRng)) {
                    env->DeleteLocalRef(bestTarget);
                    bestTarget = nullptr;
                }
            }
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

                // PATCH 1 – clamp rotation delta for TP yaw/pitch writes
                float rawYaw   = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
                float yawDelta = rawYaw - pYaw;
                while (yawDelta <= -180.0f) yawDelta += 360.0f;
                while (yawDelta >   180.0f) yawDelta -= 360.0f;
                yawDelta = std::max(-kTAMaxYawPerTick, std::min(kTAMaxYawPerTick, yawDelta));
                float clampedYaw = pYaw + yawDelta;

                // PATCH 2 – use aimHeight in pitch calculation
                double distXZ  = std::sqrt(diffX*diffX + diffZ*diffZ);
                double diffY   = (ty + aimHeight) - (py + 1.62);
                float rawPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);
                float pitchDelta = rawPitch - pPitch;
                while (pitchDelta <= -180.0f) pitchDelta += 360.0f;
                while (pitchDelta >   180.0f) pitchDelta -= 360.0f;
                pitchDelta = std::max(-kTAMaxPitchPerTick, std::min(kTAMaxPitchPerTick, pitchDelta));
                float clampedPitch = pPitch + pitchDelta;

                jobject networkHandler = env->GetObjectField(player, taNetworkHandlerField);
                if (networkHandler) {
                    double tpX = tx - (diffX / bestDist) * 2.0;
                    double tpY = ty;
                    double tpZ = tz - (diffZ / bestDist) * 2.0;

                    jobject packetTo = nullptr;
                    if (taFullPacketInitMethod) {
                        packetTo = env->NewObject(taFullPacketClass, taFullPacketInitMethod, tpX, tpY, tpZ, clampedYaw, clampedPitch, JNI_TRUE);
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
