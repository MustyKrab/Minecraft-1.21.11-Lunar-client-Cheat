#include "Killaura.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <random>
#define NOMINMAX
#include <windows.h>

static bool mappingsLoaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, interactionManagerClass, handClass, listClass;
static jclass hitResultClass, raycastContextClass, blockHitResultClass;
static jclass networkHandlerClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField;
static jfieldID entX, entY, entZ, yawField, pitchField, mainHandField;
static jfieldID networkHandlerField;
static jmethodID listSize, listGet, getHealth, attackMethod, swingMethod, getCooldownMethod;
static jmethodID raycastMethod, hitResultGetTypeMethod, getCameraPosVecMethod, getRotationVecMethod, raycastContextInitMethod;
static jmethodID sendPacketMethod;
static jclass clientPlayerClass;
static jfieldID clientNetworkHandlerField;

// PATCH 4 – target-switch cooldown state
static void*  s_kaLastTargetId   = nullptr;
static DWORD  s_kaTargetLostMs   = 0;
static bool   s_kaTargetWasActive = false;

// PATCH 5 – stochastic tick interval
static DWORD  s_kaNextTickMs = 0;

// persistent RNG
static std::mt19937 s_kaRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

static float gaussian_noise_ka(float stddev) {
    std::uniform_real_distribution<float> u(1e-6f, 1.0f);
    float u1 = u(s_kaRng), u2 = u(s_kaRng);
    return stddev * std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530f * u2);
}

Killaura::Killaura()
    : Module("Killaura"),
      reach(3.0f), fov(90.0f),
      enable360(false), packetOrderOptimize(false),
      ticksSinceLastAttack(0), randomDelay(0),
      prevPx(0.0), prevPy(0.0), prevPz(0.0), hasPrevPos(false) {}

void Killaura::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!mappingsLoaded) {
        mcClass                   = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        worldClass                = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",  "net/minecraft/client/world/ClientWorld");
        entityClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        livingClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        playerClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;", "net/minecraft/entity/player/PlayerEntity");
        interactionManagerClass   = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;",  "net/minecraft/client/network/ClientPlayerInteractionManager");
        handClass                 = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");
        listClass                 = env->FindClass("java/util/List");
        clientPlayerClass         = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;",  "net/minecraft/client/network/ClientPlayerEntity");
        networkHandlerClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;",  "net/minecraft/client/network/ClientPlayNetworkHandler");

        hitResultClass            = JNIHelper::FindClassSafe("Lnet/minecraft/class_239;",  "net/minecraft/util/hit/HitResult");
        raycastContextClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_3959;", "net/minecraft/world/RaycastContext");
        blockHitResultClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_3965;", "net/minecraft/util/hit/BlockHitResult");

        if (!mcClass || !worldClass || !entityClass || !livingClass || !playerClass ||
            !interactionManagerClass || !handClass || !clientPlayerClass || !networkHandlerClass) return;

        instanceField              = JNIHelper::GetStaticFieldSafe(mcClass,                "field_1700",  "Lnet/minecraft/class_310;",  "instance");
        playerField                = JNIHelper::GetFieldSafe(mcClass,                      "field_1724",  "Lnet/minecraft/class_746;",  "player");
        worldField                 = JNIHelper::GetFieldSafe(mcClass,                      "field_1687",  "Lnet/minecraft/class_638;",  "world");
        interactionManagerField    = JNIHelper::GetFieldSafe(mcClass,                      "field_1761",  "Lnet/minecraft/class_636;",  "interactionManager");
        playersField               = JNIHelper::GetFieldSafe(worldClass,                   "field_18226", "Ljava/util/List;",           "players");
        clientNetworkHandlerField  = JNIHelper::GetFieldSafe(clientPlayerClass,            "field_3944",  "Lnet/minecraft/class_634;",  "networkHandler");

        listSize                   = env->GetMethodID(listClass, "size", "()I");
        listGet                    = env->GetMethodID(listClass, "get",  "(I)Ljava/lang/Object;");

        entX       = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY       = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ       = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");
        yawField   = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        getHealth         = JNIHelper::GetMethodSafe(livingClass,           "method_6032", "()F",  "getHealth");
        attackMethod      = JNIHelper::GetMethodSafe(interactionManagerClass,"method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V", "attackEntity");
        swingMethod       = JNIHelper::GetMethodSafe(livingClass,           "method_6104", "(Lnet/minecraft/class_1268;)V", "swingHand");
        getCooldownMethod = JNIHelper::GetMethodSafe(playerClass,           "method_7261", "(F)F",  "getAttackCooldownProgress");
        mainHandField     = JNIHelper::GetStaticFieldSafe(handClass,        "field_5808",  "Lnet/minecraft/class_1268;", "MAIN_HAND");

        if (networkHandlerClass)
            sendPacketMethod = JNIHelper::GetMethodSafe(networkHandlerClass, "method_52787", "(Lnet/minecraft/class_2596;)V", "sendPacket");

        if (worldClass && raycastContextClass && hitResultClass) {
            raycastMethod            = JNIHelper::GetMethodSafe(worldClass,     "method_17742", "(Lnet/minecraft/class_3959;)Lnet/minecraft/class_3965;", "raycast");
            hitResultGetTypeMethod   = JNIHelper::GetMethodSafe(hitResultClass, "method_17783", "()Lnet/minecraft/class_239$class_240;",                  "getType");
            getCameraPosVecMethod    = JNIHelper::GetMethodSafe(entityClass,    "method_5865",  "(F)Lnet/minecraft/class_243;",                          "getCameraPosVec");
            getRotationVecMethod     = JNIHelper::GetMethodSafe(entityClass,    "method_5720",  "(F)Lnet/minecraft/class_243;",                          "getRotationVec");
        }

        mappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField || !playersField ||
        !entX || !entY || !entZ || !yawField || !pitchField ||
        !listSize || !listGet || !getHealth || !attackMethod || !swingMethod ||
        !getCooldownMethod || !mainHandField) return;

    // PATCH 5 – stochastic tick interval
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_kaNextTickMs) return;
        std::uniform_int_distribution<int> baseDist(35, 60);
        int base = baseDist(s_kaRng);
        int jitter = (int)gaussian_noise_ka(4.0f);
        s_kaNextTickMs = now + (DWORD)(base + jitter);
    }

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player             = env->GetObjectField(mc, playerField);
    jobject world              = env->GetObjectField(mc, worldField);
    jobject interactionManager = env->GetObjectField(mc, interactionManagerField);

    if (!player || !world || !interactionManager) {
        if (player)             env->DeleteLocalRef(player);
        if (world)              env->DeleteLocalRef(world);
        if (interactionManager) env->DeleteLocalRef(interactionManager);
        env->DeleteLocalRef(mc);
        return;
    }

    ticksSinceLastAttack++;

    {
        double px = env->GetDoubleField(player, entX);
        double py = env->GetDoubleField(player, entY);
        double pz = env->GetDoubleField(player, entZ);
        float  currentYaw = env->GetFloatField(player, yawField);

        double evalPx = (packetOrderOptimize && hasPrevPos) ? prevPx : px;
        double evalPy = (packetOrderOptimize && hasPrevPos) ? prevPy : py;
        double evalPz = (packetOrderOptimize && hasPrevPos) ? prevPz : pz;

        jobject playersList = env->GetObjectField(world, playersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, listSize);
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
        float aimHeight = heightJitter(s_kaRng);

        // small position noise (existing behaviour)
        std::uniform_real_distribution<double> randomOffset(-0.3, 0.3);

        for (int i = 0; i < size; i++) {
            jobject target = env->CallObjectMethod(playersList, listGet, i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!target) continue;

            if (env->IsSameObject(player, target)) {
                env->DeleteLocalRef(target);
                continue;
            }

            double tx = env->GetDoubleField(target, entX) + randomOffset(s_kaRng);
            double ty = env->GetDoubleField(target, entY) + aimHeight + randomOffset(s_kaRng);
            double tz = env->GetDoubleField(target, entZ) + randomOffset(s_kaRng);

            double dist = std::sqrt(
                std::pow(tx - evalPx, 2) +
                std::pow(ty - (evalPy + 1.62), 2) +
                std::pow(tz - evalPz, 2)
            );

            if (dist <= bestDist) {
                float hp = env->CallFloatMethod(target, getHealth);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
                if (hp > 0.0f) {
                    if (!enable360) {
                        double diffX    = tx - evalPx;
                        double diffZ    = tz - evalPz;
                        float targetYaw = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;

                        float yawDiff = targetYaw - currentYaw;
                        while (yawDiff <= -180.0f) yawDiff += 360.0f;
                        while (yawDiff >   180.0f) yawDiff -= 360.0f;

                        if (std::abs(yawDiff) > fov) {
                            env->DeleteLocalRef(target);
                            continue;
                        }
                    }

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
        if (bestTargetId != s_kaLastTargetId) {
            if (s_kaTargetWasActive) s_kaTargetLostMs = nowMs;
            s_kaLastTargetId      = bestTargetId;
            s_kaTargetWasActive   = (bestTarget != nullptr);
            if (bestTarget) {
                std::uniform_int_distribution<int> cdDist(150, 300);
                if (nowMs - s_kaTargetLostMs < (DWORD)cdDist(s_kaRng)) {
                    env->DeleteLocalRef(bestTarget);
                    bestTarget = nullptr;
                }
            }
        }

        if (bestTarget) {
            float cooldown = env->CallFloatMethod(player, getCooldownMethod, 0.5f);
            if (env->ExceptionCheck()) { env->ExceptionClear(); cooldown = 1.0f; }

            if (cooldown >= 1.0f) {
                if (randomDelay == 0) {
                    std::uniform_int_distribution<> delayDist(1, 4);
                    randomDelay = delayDist(s_kaRng);
                }

                if (ticksSinceLastAttack >= randomDelay) {
                    if (packetOrderOptimize && clientNetworkHandlerField && sendPacketMethod) {
                        jobject networkHandler = env->GetObjectField(player, clientNetworkHandlerField);
                        if (networkHandler) {
                            env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                            if (env->ExceptionCheck()) env->ExceptionClear();

                            jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                            if (mainHand) {
                                env->CallVoidMethod(player, swingMethod, mainHand);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                env->DeleteLocalRef(mainHand);
                            }
                            env->DeleteLocalRef(networkHandler);
                        } else {
                            env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                            if (mainHand) {
                                env->CallVoidMethod(player, swingMethod, mainHand);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                env->DeleteLocalRef(mainHand);
                            }
                        }
                    } else {
                        env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
                        if (mainHand) {
                            env->CallVoidMethod(player, swingMethod, mainHand);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            env->DeleteLocalRef(mainHand);
                        }
                    }

                    ticksSinceLastAttack = 0;
                    randomDelay          = 0;
                }
            }
            env->DeleteLocalRef(bestTarget);
        }

        env->DeleteLocalRef(playersList);

        prevPx    = px;
        prevPy    = py;
        prevPz    = pz;
        hasPrevPos = true;
    }

cleanup:
    env->DeleteLocalRef(interactionManager);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
