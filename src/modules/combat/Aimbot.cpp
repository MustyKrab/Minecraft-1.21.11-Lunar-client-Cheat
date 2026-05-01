#include "Aimbot.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <random>
#include <algorithm>

// ─ static mappings ──────────────────────────────────────────────────────────
static bool aimbot_mappings_loaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, optionsClass, doubleOptionClass;
static jfieldID instanceField, playerField, worldField, playersField, optionsField, sensitivityField;
static jfieldID entX, entY, entZ, yawField, pitchField;
static jmethodID listSize, listGet, getHealth, getDoubleValue;

// cached for GCD block – set once in mappings load, never again
static jclass    s_dblClass  = nullptr;
static jmethodID s_dblValMID = nullptr;
static jmethodID getIdMethod;

// ── persistent RNG (seeded once, never re-seeded in hot path) ──────────────
static std::mt19937 s_rng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

// ── per-tick state for humanised motion ────────────────────────────────────
static float  s_yawVel    = 0.0f;
static float  s_pitchVel  = 0.0f;
static DWORD  s_nextTickMs = 0;   // earliest ms we are allowed to act again

// PATCH 4 — target-switch re-acquisition cooldown state
static jint   s_lastTargetId    = -1;
static DWORD  s_targetLostMs    = 0;
static bool   s_targetWasActive = false;

// PATCH 1 — per-tick rotation velocity caps (degrees/tick)
static constexpr float kMaxYawPerTick   = 18.0f;
static constexpr float kMaxPitchPerTick = 12.0f;

// ── helpers ────────────────────────────────────────────────────────────────

// Gaussian-ish noise via Box-Muller (cheap, no trig branch predictor issues)
static float gaussian_noise(float stddev) {
    std::uniform_real_distribution<float> u(1e-6f, 1.0f);
    float u1 = u(s_rng), u2 = u(s_rng);
    return stddev * std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530f * u2);
}

// Normalise angle to (-180, 180]
static inline float wrap180(float a) {
    while (a <= -180.0f) a += 360.0f;
    while (a >   180.0f) a -= 360.0f;
    return a;
}

// ── ctor ───────────────────────────────────────────────────────────────────
Aimbot::Aimbot() : Module("Aimbot"), fov(90.0f), smoothSpeed(0.15f) {}

// ── OnTick ─────────────────────────────────────────────────────────────────
void Aimbot::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    // ─ lazy mapping load ───────────────────────────────────────────────────
    if (!aimbot_mappings_loaded) {
        mcClass          = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",    "net/minecraft/client/MinecraftClient");
        worldClass       = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",    "net/minecraft/client/world/ClientWorld");
        entityClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;",   "net/minecraft/entity/Entity");
        livingClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_1309;",   "net/minecraft/entity/LivingEntity");
        playerClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_1657;",   "net/minecraft/entity/player/PlayerEntity");
        optionsClass     = JNIHelper::FindClassSafe("Lnet/minecraft/class_315;",    "net/minecraft/client/option/GameOptions");
        doubleOptionClass= JNIHelper::FindClassSafe("Lnet/minecraft/class_7172;",   "net/minecraft/client/option/SimpleOption");

        jclass listClass = env->FindClass("java/util/List");

        if (!mcClass || !worldClass || !entityClass || !livingClass ||
            !playerClass || !optionsClass || !doubleOptionClass) return;

        instanceField    = JNIHelper::GetStaticFieldSafe(mcClass,    "field_1700",  "Lnet/minecraft/class_310;",  "instance");
        playerField      = JNIHelper::GetFieldSafe(mcClass,          "field_1724",  "Lnet/minecraft/class_746;",  "player");
        worldField       = JNIHelper::GetFieldSafe(mcClass,          "field_1687",  "Lnet/minecraft/class_638;",  "world");
        optionsField     = JNIHelper::GetFieldSafe(mcClass,          "field_1690",  "Lnet/minecraft/class_315;",  "options");
        playersField     = JNIHelper::GetFieldSafe(worldClass,       "field_18226", "Ljava/util/List;",           "players");

        listSize         = env->GetMethodID(listClass, "size", "()I");
        listGet          = env->GetMethodID(listClass, "get",  "(I)Ljava/lang/Object;");

        entX             = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY             = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ             = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");
        yawField         = JNIHelper::GetFieldSafe(entityClass, "field_5982", "F", "yaw");
        pitchField       = JNIHelper::GetFieldSafe(entityClass, "field_5965", "F", "pitch");

        getHealth        = JNIHelper::GetMethodSafe(livingClass,      "method_6032", "()F",  "getHealth");
        sensitivityField = JNIHelper::GetFieldSafe(optionsClass,      "field_1844",  "Lnet/minecraft/class_7172;", "mouseSensitivity");
        getDoubleValue   = JNIHelper::GetMethodSafe(doubleOptionClass,"method_41753","()Ljava/lang/Object;",       "getValue");

        jclass localDbl  = env->FindClass("java/lang/Double");
        if (localDbl) {
            s_dblClass  = (jclass)env->NewGlobalRef(localDbl);
            s_dblValMID = env->GetMethodID(s_dblClass, "doubleValue", "()D");
            env->DeleteLocalRef(localDbl);
        }

        getIdMethod = env->GetMethodID(entityClass, "getId", "()I");

        aimbot_mappings_loaded = true;
    }

    if (!instanceField || !playerField || !worldField || !optionsField ||
        !playersField   || !entX       || !entY       || !entZ         ||
        !yawField       || !pitchField || !listSize   || !listGet      || !getHealth)
        return;

    // ─ LMB gate ───────────────────────────────────────────────────────────
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        // PATCH 3 — randomised velocity bleed multiplier on release
        std::uniform_real_distribution<float> bleedDist(0.5f, 0.7f);
        float bleed = bleedDist(s_rng);
        s_yawVel   *= bleed;
        s_pitchVel *= bleed;
        return;
    }

    // ─ PATCH 5 — stochastic tick-rate jitter ──────────────────────────────
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_nextTickMs) return;

        // base: uniform(35,60) ms + gaussian jitter
        std::uniform_int_distribution<int> baseDist(35, 60);
        int base   = baseDist(s_rng);
        int jitter = (int)gaussian_noise(4.0f);
        s_nextTickMs = now + (DWORD)(base + jitter);
    }

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player  = env->GetObjectField(mc, playerField);
    jobject world   = env->GetObjectField(mc, worldField);
    jobject options = env->GetObjectField(mc, optionsField);

    if (!player || !world || !options) {
        if (player)  env->DeleteLocalRef(player);
        if (world)   env->DeleteLocalRef(world);
        if (options) env->DeleteLocalRef(options);
        env->DeleteLocalRef(mc);
        return;
    }

    {
        double px = env->GetDoubleField(player, entX);
        double py = env->GetDoubleField(player, entY);
        double pz = env->GetDoubleField(player, entZ);
        float  currentYaw   = env->GetFloatField(player, yawField);
        float  currentPitch = env->GetFloatField(player, pitchField);

        double eyePy = py + 1.62;

        jobject playersList = env->GetObjectField(world, playersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, listSize);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(playersList); goto cleanup; }

        jobject  bestTarget   = nullptr;
        float    bestAngDist  = fov;
        double   bestDist3D   = 6.0;
        jint     bestTargetId = -1;

        // PATCH 2 — randomised aim-height jitter per scan
        std::uniform_real_distribution<float> heightJitter(0.6f, 1.4f);
        float aimHeight = heightJitter(s_rng);

        for (int i = 0; i < size; i++) {
            jobject target = env->CallObjectMethod(playersList, listGet, i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!target) continue;

            if (env->IsSameObject(player, target)) { env->DeleteLocalRef(target); continue; }

            double tx = env->GetDoubleField(target, entX);
            double ty = env->GetDoubleField(target, entY);
            double tz = env->GetDoubleField(target, entZ);

            double diffX  = tx - px;
            double diffY  = (ty + aimHeight) - eyePy;
            double diffZ  = tz - pz;
            double dist3D = std::sqrt(diffX*diffX + diffY*diffY + diffZ*diffZ);
            double distXZ = std::sqrt(diffX*diffX + diffZ*diffZ);

            float hp = env->CallFloatMethod(target, getHealth);
            if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
            if (hp <= 0.0f) { env->DeleteLocalRef(target); continue; }

            float tYaw   = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
            float tPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);

            float yDiff  = wrap180(tYaw   - currentYaw);
            float pDiff  = wrap180(tPitch - currentPitch);
            float angDist = std::sqrt(yDiff*yDiff + pDiff*pDiff);

            // FIX: Ensure absolute value is used correctly for FOV check
            if (angDist < (fov / 2.0f)) { // FOV is total angle, so check against half
                if (angDist < bestAngDist || (angDist == bestAngDist && dist3D < bestDist3D)) {
                    bestAngDist  = angDist;
                    bestDist3D   = dist3D;
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
        if (bestTargetId != s_lastTargetId) {
            // target switched or lost
            if (s_targetWasActive) {
                s_targetLostMs = nowMs;
            }
            s_lastTargetId    = bestTargetId;
            s_targetWasActive = (bestTarget != nullptr);

            if (bestTarget) {
                // gate re-acquisition for 150-300ms
                std::uniform_int_distribution<int> cooldownDist(150, 300);
                DWORD cooldown = (DWORD)cooldownDist(s_rng);
                if (nowMs - s_targetLostMs < cooldown) {
                    env->DeleteLocalRef(bestTarget);
                    bestTarget = nullptr;
                }
            }
        }

        if (bestTarget) {
            double tx = env->GetDoubleField(bestTarget, entX);
            double ty = env->GetDoubleField(bestTarget, entY);
            double tz = env->GetDoubleField(bestTarget, entZ);

            // PATCH 2 — same jitter value for the actual aim computation
            double diffX  = tx - px;
            double diffY  = (ty + aimHeight) - eyePy;
            double diffZ  = tz - pz;
            double distXZ = std::sqrt(diffX*diffX + diffZ*diffZ);

            float targetYaw   = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
            float targetPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);

            float yawDiff   = wrap180(targetYaw   - currentYaw);
            float pitchDiff = wrap180(targetPitch - currentPitch);

            // GCD compensation BEFORE lerp
            float gcd = 1.0f;
            if (sensitivityField && getDoubleValue) {
                jobject sensObj = env->GetObjectField(options, sensitivityField);
                if (sensObj) {
                    jobject sensDoubleObj = env->CallObjectMethod(sensObj, getDoubleValue);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (sensDoubleObj) {
                        double sens = env->CallDoubleMethod(sensDoubleObj, s_dblValMID);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); sens = 0.5; }
                        float f = (float)(sens * 0.6 + 0.2);
                        gcd = f * f * f * 1.2f;
                        env->DeleteLocalRef(sensDoubleObj);
                    }
                    env->DeleteLocalRef(sensObj);
                }
            }

            float yawRem = std::fmod(yawDiff, gcd);
            if (yawRem < 0) yawRem += gcd;
            yawDiff -= yawRem;

            float pitchRem = std::fmod(pitchDiff, gcd);
            if (pitchRem < 0) pitchRem += gcd;
            pitchDiff -= pitchRem;

            // spring-damper velocity model
            float baseSpeed = smoothSpeed;
            float angDist   = std::sqrt(yawDiff*yawDiff + pitchDiff*pitchDiff);
            float distScale = (std::min)(1.0f, angDist / 15.0f);
            float targetSpeed = baseSpeed * (0.55f + 0.45f * distScale);
            targetSpeed += gaussian_noise(baseSpeed * 0.07f);
            targetSpeed  = (std::max)(0.02f, (std::min)(targetSpeed, 0.95f));

            s_yawVel   += (yawDiff   * targetSpeed - s_yawVel)   * 0.35f;
            s_pitchVel += (pitchDiff * targetSpeed - s_pitchVel) * 0.35f;

            s_yawVel   += gaussian_noise(0.012f);
            s_pitchVel += gaussian_noise(0.008f);

            // PATCH 1 — clamp velocity before writing rotation fields
            s_yawVel   = (std::max)(-kMaxYawPerTick,   (std::min)(kMaxYawPerTick,   s_yawVel));
            s_pitchVel = (std::max)(-kMaxPitchPerTick, (std::min)(kMaxPitchPerTick, s_pitchVel));

            float newYaw   = currentYaw   + s_yawVel;
            float newPitch = currentPitch + s_pitchVel;

            newPitch = (std::max)(-90.0f, (std::min)(90.0f, newPitch));

            env->SetFloatField(player, yawField,   newYaw);
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
