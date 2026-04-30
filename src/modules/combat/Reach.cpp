#include "Reach.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#include <windows.h>
#include <random>

static bool reachMappingsLoaded = false;
static jclass mcClass, playerClass, worldClass, interactionManagerClass, entityClass, livingClass, handClass;
static jfieldID instanceField, playerField, worldField, interactionManagerField, playersField;
static jfieldID entX, entY, entZ, mainHandField;
// FIX: cache yaw/pitch fields at mapping load time instead of re-fetching every tick
static jfieldID yawField, pitchField;
static jmethodID listSize, listGet, attackMethod, swingMethod, getHealth;

// PATCH 4 — target-switch cooldown state
// FIX: use jint identity (entity ID) instead of raw pointer cast to void* — local refs are invalidated each tick
static jint   s_reachLastTargetId    = -1;
static DWORD  s_reachTargetLostMs    = 0;
static bool   s_reachTargetWasActive = false;

// PATCH 5 — stochastic tick interval
static DWORD  s_reachNextTickMs = 0;

static std::mt19937 s_reachRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

static float gaussian_noise_reach(float stddev) {
    std::uniform_real_distribution<float> u(1e-6f, 1.0f);
    float u1 = u(s_reachRng), u2 = u(s_reachRng);
    return stddev * std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530f * u2);
}

Reach::Reach() : Module("Reach"), reachDistance(3.5f) {}

void Reach::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!reachMappingsLoaded) {
        mcClass                  = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        playerClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;",  "net/minecraft/client/network/ClientPlayerEntity");
        worldClass               = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",  "net/minecraft/client/world/ClientWorld");
        interactionManagerClass  = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;",  "net/minecraft/client/network/ClientPlayerInteractionManager");
        entityClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        livingClass              = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;", "net/minecraft/entity/LivingEntity");
        handClass                = JNIHelper::FindClassSafe("Lnet/minecraft/class_1268;", "net/minecraft/util/Hand");

        if (!mcClass || !playerClass || !worldClass || !interactionManagerClass || !entityClass || !livingClass || !handClass) return;

        instanceField           = JNIHelper::GetStaticFieldSafe(mcClass,                  "field_1700",  "Lnet/minecraft/class_310;",  "instance");
        playerField             = JNIHelper::GetFieldSafe(mcClass,                        "field_1724",  "Lnet/minecraft/class_746;",  "player");
        worldField              = JNIHelper::GetFieldSafe(mcClass,                        "field_1687",  "Lnet/minecraft/class_638;",  "world");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass,                        "field_1761",  "Lnet/minecraft/class_636;",  "interactionManager");
        playersField            = JNIHelper::GetFieldSafe(worldClass,                     "field_18226", "Ljava/util/List;",           "players");

        jclass listClass = env->FindClass("java/util/List");
        listSize = env->GetMethodID(listClass, "size", "()I");
        listGet  = env->GetMethodID(listClass, "get",  "(I)Ljava/lang/Object;");

        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        // FIX: cache these here — was re-fetching inside OnTick body every call
        yawField   = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        getHealth    = JNIHelper::GetMethodSafe(livingClass,             "method_6032", "()F",                                                        "getHealth");
        attackMethod = JNIHelper::GetMethodSafe(interactionManagerClass, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V",     "attackEntity");
        swingMethod  = JNIHelper::GetMethodSafe(livingClass,             "method_6104", "(Lnet/minecraft/class_1268;)V",                              "swingHand");
        mainHandField= JNIHelper::GetStaticFieldSafe(handClass,          "field_5808",  "Lnet/minecraft/class_1268;",                                 "MAIN_HAND");

        reachMappingsLoaded = true;
    }

    if (!instanceField || !playerField || !worldField || !interactionManagerField || !playersField ||
        !entX || !entY || !entZ || !yawField || !pitchField ||
        !listSize || !listGet || !getHealth || !attackMethod || !swingMethod || !mainHandField) return;

    static bool wasClicked = false;
    bool isClicked = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (!isClicked || wasClicked) {
        wasClicked = isClicked;
        return;
    }
    wasClicked = true;

    // PATCH 5 — stochastic tick interval
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_reachNextTickMs) return;
        std::uniform_int_distribution<int> baseDist(35, 60);
        int base   = baseDist(s_reachRng);
        int jitter = (int)gaussian_noise_reach(4.0f);
        s_reachNextTickMs = now + (DWORD)(base + jitter);
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

    {
        double px = env->GetDoubleField(player, entX);
        double py = env->GetDoubleField(player, entY) + 1.62;
        double pz = env->GetDoubleField(player, entZ);

        // FIX: yawField/pitchField are now cached — no GetFieldSafe call here
        float yaw   = env->GetFloatField(player, yawField);
        float pitch = env->GetFloatField(player, pitchField);

        float f = pitch * 0.01745329F;
        float g = -yaw  * 0.01745329F;
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
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(playersList);
            goto cleanup;
        }

        jobject bestTarget   = nullptr;
        jint    bestTargetId = -1; // FIX: use entity ID (jint) for identity, not raw pointer

        // PATCH 2 — randomised aim-height jitter
        std::uniform_real_distribution<float> heightJitter(0.6f, 1.4f);
        float aimHeight = heightJitter(s_reachRng);

        std::uniform_real_distribution<float> reachDist(reachDistance - 0.1f, reachDistance + 0.1f);
        double actualReach = reachDist(s_reachRng);
        double bestDist    = actualReach;

        // FIX: need entity ID field — cache at mapping load; use getEntityId method as fallback
        jmethodID getIdMethod = env->GetMethodID(entityClass, "getId", "()I");

        for (int idx = 0; idx < size; idx++) {
            jobject target = env->CallObjectMethod(playersList, listGet, idx);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!target) continue;

            if (env->IsSameObject(player, target)) {
                env->DeleteLocalRef(target);
                continue;
            }

            float hp = env->CallFloatMethod(target, getHealth);
            if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
            if (hp <= 0.0f) { env->DeleteLocalRef(target); continue; }

            double tx = env->GetDoubleField(target, entX);
            double ty = env->GetDoubleField(target, entY) + aimHeight;
            double tz = env->GetDoubleField(target, entZ);

            double diffX = tx - px;
            double diffY = ty - py;
            double diffZ = tz - pz;
            double dist  = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);

            // FIX: removed dist > 3.0 hardcoded lower bound — was blocking targets closer than 3 blocks
            if (dist <= bestDist) {
                double dot = (diffX * lookX + diffY * lookY + diffZ * lookZ) / dist;
                if (dot > 0.95) {
                    bestDist = dist;
                    // FIX: get actual entity ID for stable cross-tick identity comparison
                    bestTargetId = getIdMethod ? env->CallIntMethod(target, getIdMethod) : -1;
                    if (env->ExceptionCheck()) { env->ExceptionClear(); bestTargetId = -1; }
                    if (bestTarget) env->DeleteLocalRef(bestTarget);
                    bestTarget = env->NewLocalRef(target);
                }
            }
            env->DeleteLocalRef(target);
        }

        // PATCH 4 — target-switch cooldown
        DWORD nowMs = (DWORD)GetTickCount64();
        if (bestTargetId != s_reachLastTargetId) {
            if (s_reachTargetWasActive) s_reachTargetLostMs = nowMs;
            s_reachLastTargetId    = bestTargetId;
            s_reachTargetWasActive = (bestTarget != nullptr);
            if (bestTarget) {
                std::uniform_int_distribution<int> cdDist(150, 300);
                if (nowMs - s_reachTargetLostMs < (DWORD)cdDist(s_reachRng)) {
                    env->DeleteLocalRef(bestTarget);
                    bestTarget = nullptr;
                }
            }
        }

        if (bestTarget) {
            env->CallVoidMethod(interactionManager, attackMethod, player, bestTarget);
            if (env->ExceptionCheck()) env->ExceptionClear();

            jobject mainHand = env->GetStaticObjectField(handClass, mainHandField);
            if (mainHand) {
                env->CallVoidMethod(player, swingMethod, mainHand);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(mainHand);
            }
            env->DeleteLocalRef(bestTarget);
        }

        // FIX: playersList deleted here before goto — was leaked on yawField/pitchField failure path
        env->DeleteLocalRef(playersList);
    }

cleanup:
    env->DeleteLocalRef(interactionManager);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
