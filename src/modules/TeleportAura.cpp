#include "TeleportAura.h"
#include "../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <random>

static bool taMappingsLoaded = false;
static jclass taMcClass, taWorldClass, taEntityClass, taLivingClass, taPlayerClass,
              taInteractionManagerClass, taHandClass, taListClass;
static jclass taNetworkHandlerClass, taPositionAndOnGroundClass, taFullPacketClass;
static jfieldID taInstanceField, taPlayerField, taWorldField, taInteractionManagerField,
                taPlayersField, taNetworkHandlerField;
static jfieldID taEntX, taEntY, taEntZ, taYaw, taPitch, taMainHandField;
static jmethodID taListSize, taListGet, taGetHealth, taAttackMethod, taSwingMethod,
                 taGetCooldownMethod, taSendPacketMethod, taPacketInitMethod, taFullPacketInitMethod;
// FIX: cache getId method
static jmethodID taGetIdMethod;

// rotation velocity caps
static constexpr float kTAMaxYawPerTick   = 18.0f;
static constexpr float kTAMaxPitchPerTick = 12.0f;

// target-switch cooldown
// FIX: use jint identity (entity ID) instead of raw pointer cast to void*
static jint   s_taLastTargetId     = -1;
static DWORD  s_taTargetLostMs     = 0;
static bool   s_taTargetWasActive  = false;

// stochastic tick interval
static DWORD s_taNextTickMs = 0;

// packet-back delay: don't send return packet same tick as attack
static DWORD s_taPendingBackMs = 0;
static double s_taPendingPx = 0, s_taPendingPy = 0, s_taPendingPz = 0;
static float  s_taPendingYaw = 0, s_taPendingPitch = 0;
static bool   s_taHasPendingBack = false;

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
        taMcClass                  = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",    "net/minecraft/client/MinecraftClient");
        taWorldClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",    "net/minecraft/client/world/ClientWorld");
        taEntityClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;",   "net/minecraft/entity/Entity");
        taLivingClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;",   "net/minecraft/entity/LivingEntity");
        taPlayerClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;",   "net/minecraft/entity/player/PlayerEntity");
        taInteractionManagerClass  = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;",    "net/minecraft/client/network/ClientPlayerInteractionManager");
        taHandClass                = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;",   "net/minecraft/util/Hand");
        taListClass                = env->FindClass("java/util/List");
        taNetworkHandlerClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;",    "net/minecraft/client/network/ClientPlayNetworkHandler");
        taPositionAndOnGroundClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2829;",
                                                              "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$PositionAndOnGround");
        taFullPacketClass          = JNIHelper::FindClassSafe("Lnet/minecraft/class_2828$class_2830;",
                                                              "net/minecraft/network/packet/c2s/play/PlayerMoveC2SPacket$Full");

        if (!taMcClass || !taWorldClass || !taEntityClass || !taLivingClass || !taPlayerClass ||
            !taInteractionManagerClass || !taHandClass || !taNetworkHandlerClass ||
            !taPositionAndOnGroundClass || !taFullPacketClass) return;

        taInstanceField           = JNIHelper::GetStaticFieldSafe(taMcClass,    "field_1700",  "Lnet/minecraft/class_310;",  "instance");
        taPlayerField             = JNIHelper::GetFieldSafe(taMcClass,          "field_1724",  "Lnet/minecraft/class_746;",  "player");
        taWorldField              = JNIHelper::GetFieldSafe(taMcClass,          "field_1687",  "Lnet/minecraft/class_638;",  "world");
        taInteractionManagerField = JNIHelper::GetFieldSafe(taMcClass,          "field_1761",  "Lnet/minecraft/class_636;",  "interactionManager");
        taNetworkHandlerField     = JNIHelper::GetFieldSafe(
                                        JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity"),
                                        "field_3944", "Lnet/minecraft/class_634;", "networkHandler");
        taPlayersField            = JNIHelper::GetFieldSafe(taWorldClass,       "field_18226", "Ljava/util/List;",           "players");

        taListSize            = env->GetMethodID(taListClass, "size", "()I");
        taListGet             = env->GetMethodID(taListClass, "get",  "(I)Ljava/lang/Object;");

        taEntX  = JNIHelper::GetFieldSafe(taEntityClass, "field_6014", "D", "x");
        taEntY  = JNIHelper::GetFieldSafe(taEntityClass, "field_6036", "D", "y");
        taEntZ  = JNIHelper::GetFieldSafe(taEntityClass, "field_5969", "D", "z");
        taYaw   = JNIHelper::GetFieldSafe(taEntityClass, "field_5982", "F", "yaw");
        taPitch = JNIHelper::GetFieldSafe(taEntityClass, "field_5965", "F", "pitch");

        taGetHealth          = JNIHelper::GetMethodSafe(taLivingClass,             "method_6032",  "()F",                                        "getHealth");
        taAttackMethod       = JNIHelper::GetMethodSafe(taInteractionManagerClass, "method_2918",  "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V",                "attackEntity");
        taSwingMethod        = JNIHelper::GetMethodSafe(taLivingClass,             "method_6104",  "(Lnet/minecraft/class_1268;)V",                                          "swingHand");
        taGetCooldownMethod  = JNIHelper::GetMethodSafe(taPlayerClass,             "method_7261",  "(F)F",                                                                   "getAttackCooldownProgress");
        taSendPacketMethod   = JNIHelper::GetMethodSafe(taNetworkHandlerClass,     "method_52787", "(Lnet/minecraft/class_2596;)V",                                          "sendPacket");

        taPacketInitMethod = env->GetMethodID(taPositionAndOnGroundClass, "<init>", "(DDDZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taPacketInitMethod = nullptr; }

        taFullPacketInitMethod = env->GetMethodID(taFullPacketClass, "<init>", "(DDDFFZ)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); taFullPacketInitMethod = nullptr; }

        taMainHandField = JNIHelper::GetStaticFieldSafe(taHandClass, "field_5808", "Lnet/minecraft/class_1268;", "MAIN_HAND");

        // FIX: cache getId method
        taGetIdMethod = env->GetMethodID(taEntityClass, "getId", "()I");

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
        !taMainHandField || !taNetworkHandlerField || !taSendPacketMethod) return;

    if (!taPacketInitMethod && !taFullPacketInitMethod) return;

    // stochastic tick interval
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_taNextTickMs) {
            // still service any pending back-packet while waiting
            goto service_back;
        }
        std::uniform_int_distribution<int> baseDist(35, 60);
        int base   = baseDist(s_taRng);
        int jitter = (int)gaussian_noise_ta(4.0f);
        s_taNextTickMs = now + (DWORD)(base + jitter);
    }

    {
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
            double px   = env->GetDoubleField(player, taEntX);
            double py   = env->GetDoubleField(player, taEntY);
            double pz   = env->GetDoubleField(player, taEntZ);
            float  pYaw = env->GetFloatField(player,  taYaw);
            float  pPitch = env->GetFloatField(player, taPitch);

            jobject playersList = env->GetObjectField(world, taPlayersField);
            if (!playersList) goto cleanup_inner;

            int size = env->CallIntMethod(playersList, taListSize);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                env->DeleteLocalRef(playersList);
                goto cleanup_inner;
            }

            jobject   bestTarget   = nullptr;
            // FIX: init to reach so anything within range wins on first hit
            double    bestDist     = (double)reach;
            jint      bestTargetId = -1;

            // aim-height jitter — sampled once per scan
            std::uniform_real_distribution<float> heightJitter(0.6f, 1.4f);
            float aimHeight = heightJitter(s_taRng);

            for (int i = 0; i < size; i++) {
                jobject target = env->CallObjectMethod(playersList, taListGet, i);
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (!target) continue;

                if (env->IsSameObject(player, target)) { env->DeleteLocalRef(target); continue; }

                double tx = env->GetDoubleField(target, taEntX);
                double ty = env->GetDoubleField(target, taEntY);
                double tz = env->GetDoubleField(target, taEntZ);

                double dist = std::sqrt(std::pow(tx - px, 2) + std::pow(ty - py, 2) + std::pow(tz - pz, 2));

                // FIX: lower bound 3.0 was blocking close targets — use 0.5 (melee min)
                if (dist <= bestDist && dist > 0.5) {
                    float hp = env->CallFloatMethod(target, taGetHealth);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
                    if (hp > 0.0f) {
                        bestDist     = dist;
                        // FIX: use entity ID
                        bestTargetId = taGetIdMethod ? env->CallIntMethod(target, taGetIdMethod) : -1;
                        if (env->ExceptionCheck()) { env->ExceptionClear(); bestTargetId = -1; }
                        if (bestTarget) env->DeleteLocalRef(bestTarget);
                        bestTarget = env->NewLocalRef(target);
                    }
                }
                env->DeleteLocalRef(target);
            }

            // target-switch cooldown
            DWORD nowMs = (DWORD)GetTickCount64();
            if (bestTargetId != s_taLastTargetId) {
                if (s_taTargetWasActive) s_taTargetLostMs = nowMs;
                s_taLastTargetId     = bestTargetId;
                s_taTargetWasActive  = (bestTarget != nullptr);
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

                    // clamped yaw
                    float rawYaw   = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
                    float yawDelta = rawYaw - pYaw;
                    while (yawDelta <= -180.0f) yawDelta += 360.0f;
                    while (yawDelta >   180.0f) yawDelta -= 360.0f;
                    yawDelta = std::max(-kTAMaxYawPerTick, std::min(kTAMaxYawPerTick, yawDelta));
                    float clampedYaw = pYaw + yawDelta;

                    // clamped pitch with aimHeight jitter
                    double distXZ   = std::sqrt(diffX*diffX + diffZ*diffZ);
                    double diffY    = (ty + aimHeight) - (py + 1.62);
                    float rawPitch  = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);
                    float pitchDelta = rawPitch - pPitch;
                    while (pitchDelta <= -180.0f) pitchDelta += 360.0f;
                    while (pitchDelta >   180.0f) pitchDelta -= 360.0f;
                    pitchDelta = std::max(-kTAMaxPitchPerTick, std::min(kTAMaxPitchPerTick, pitchDelta));
                    float clampedPitch = pPitch + pitchDelta;

                    // TP offset: land 2 blocks in front of target + small XZ jitter
                    std::uniform_real_distribution<float> offsetJitter(-0.3f, 0.3f);
                    double tpX = tx - (diffX / bestDist) * 2.0 + offsetJitter(s_taRng);
                    double tpY = ty;
                    double tpZ = tz - (diffZ / bestDist) * 2.0 + offsetJitter(s_taRng);

                    jobject networkHandler = env->GetObjectField(player, taNetworkHandlerField);
                    if (networkHandler) {
                        // send TP-to packet
                        jobject packetTo = nullptr;
                        if (taFullPacketInitMethod) {
                            packetTo = env->NewObject(taFullPacketClass, taFullPacketInitMethod,
                                                      tpX, tpY, tpZ, clampedYaw, clampedPitch, JNI_TRUE);
                        } else if (taPacketInitMethod) {
                            packetTo = env->NewObject(taPositionAndOnGroundClass, taPacketInitMethod,
                                                      tpX, tpY, tpZ, JNI_TRUE);
                        }
                        if (env->ExceptionCheck()) { env->ExceptionClear(); packetTo = nullptr; }

                        if (packetTo) {
                            env->CallVoidMethod(networkHandler, taSendPacketMethod, packetTo);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(packetTo);

                            // attack + swing
                            env->CallVoidMethod(interactionManager, taAttackMethod, player, bestTarget);
                            if (env->ExceptionCheck()) env->ExceptionClear();

                            jobject mainHand = env->GetStaticObjectField(taHandClass, taMainHandField);
                            if (mainHand) {
                                // jitter swing timing: 0-40 ms after attack
                                std::uniform_int_distribution<int> swingDelay(0, 40);
                                DWORD swingAt = nowMs + (DWORD)swingDelay(s_taRng);
                                if (nowMs >= swingAt) {
                                    env->CallVoidMethod(player, taSwingMethod, mainHand);
                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                }
                                env->DeleteLocalRef(mainHand);
                            }

                            // queue back-packet for next tick (50-120ms later) — avoids
                            // same-tick teleport-back which is a trivial AC signature
                            std::uniform_int_distribution<int> backDelay(50, 120);
                            s_taPendingBackMs  = nowMs + (DWORD)backDelay(s_taRng);
                            s_taPendingPx      = px;
                            s_taPendingPy      = py;
                            s_taPendingPz      = pz;
                            s_taPendingYaw     = pYaw;
                            s_taPendingPitch   = pPitch;
                            s_taHasPendingBack = true;
                        }
                        env->DeleteLocalRef(networkHandler);
                    }
                }
                // FIX: delete bestTarget local ref to prevent memory leak
                env->DeleteLocalRef(bestTarget);
            }
            env->DeleteLocalRef(playersList);
        }

cleanup_inner:
        env->DeleteLocalRef(interactionManager);
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

service_back:
    // send deferred return-to-origin packet
    if (s_taHasPendingBack) {
        DWORD now2 = (DWORD)GetTickCount64();
        if (now2 >= s_taPendingBackMs) {
            s_taHasPendingBack = false;

            jobject mc2 = env->GetStaticObjectField(taMcClass, taInstanceField);
            if (!mc2) return;
            jobject player2 = env->GetObjectField(mc2, taPlayerField);
            if (!player2) { env->DeleteLocalRef(mc2); return; }
            jobject nh2 = env->GetObjectField(player2, taNetworkHandlerField);
            if (nh2) {
                jobject packetBack = nullptr;
                if (taFullPacketInitMethod) {
                    packetBack = env->NewObject(taFullPacketClass, taFullPacketInitMethod,
                                                s_taPendingPx, s_taPendingPy, s_taPendingPz,
                                                s_taPendingYaw, s_taPendingPitch, JNI_TRUE);
                } else if (taPacketInitMethod) {
                    packetBack = env->NewObject(taPositionAndOnGroundClass, taPacketInitMethod,
                                                s_taPendingPx, s_taPendingPy, s_taPendingPz, JNI_TRUE);
                }
                if (env->ExceptionCheck()) { env->ExceptionClear(); packetBack = nullptr; }
                if (packetBack) {
                    env->CallVoidMethod(nh2, taSendPacketMethod, packetBack);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(packetBack);
                }
                env->DeleteLocalRef(nh2);
            }
            env->DeleteLocalRef(player2);
            env->DeleteLocalRef(mc2);
        }
    }
}
