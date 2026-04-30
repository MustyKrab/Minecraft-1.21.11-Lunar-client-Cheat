#include "Aimbot.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <random>
#include <algorithm>

// ── static mappings ──────────────────────────────────────────────────────────
static bool aimbot_mappings_loaded = false;
static jclass mcClass, worldClass, entityClass, livingClass, playerClass, optionsClass, doubleOptionClass;
static jfieldID instanceField, playerField, worldField, playersField, optionsField, sensitivityField;
static jfieldID entX, entY, entZ, yawField, pitchField;
static jmethodID listSize, listGet, getHealth, getDoubleValue;

// ── persistent RNG (seeded once, never re-seeded in hot path) ────────────────
static std::mt19937 s_rng([]() -> uint32_t {
    // mix two independent entropy sources so the seed itself isn't trivially
    // guessable from timing alone
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

// ── per-tick state for humanised motion ──────────────────────────────────────
static float  s_yawVel   = 0.0f;   // current angular velocity (degrees/tick)
static float  s_pitchVel = 0.0f;
static DWORD  s_nextTickMs = 0;    // earliest ms we are allowed to act again

// ── helpers ──────────────────────────────────────────────────────────────────

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

// ── ctor ─────────────────────────────────────────────────────────────────────
Aimbot::Aimbot() : Module("Aimbot"), fov(90.0f), smoothSpeed(0.15f) {}

// ── OnTick ───────────────────────────────────────────────────────────────────
void Aimbot::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    // ── lazy mapping load ────────────────────────────────────────────────────
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

        getHealth        = JNIHelper::GetMethodSafe(livingClass,      "method_6032", "()F", "getHealth");
        sensitivityField = JNIHelper::GetFieldSafe(optionsClass,      "field_1844",  "Lnet/minecraft/class_7172;", "mouseSensitivity");
        getDoubleValue   = JNIHelper::GetMethodSafe(doubleOptionClass,"method_41753","()Ljava/lang/Object;",       "getValue");

        aimbot_mappings_loaded = true;
    }

    if (!instanceField || !playerField || !worldField || !optionsField ||
        !playersField   || !entX       || !entY       || !entZ         ||
        !yawField       || !pitchField || !listSize   || !listGet      || !getHealth)
        return;

    // ── LMB gate ─────────────────────────────────────────────────────────────
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        // bleed velocity back to zero so we don't snap when re-engaging
        s_yawVel   *= 0.6f;
        s_pitchVel *= 0.6f;
        return;
    }

    // ── stochastic tick-rate jitter ──────────────────────────────────────────
    // Humans don't react at a perfectly fixed interval.  Skip ~10-20 % of ticks
    // at random so the rotation update cadence is irregular.
    {
        DWORD now = (DWORD)GetTickCount64();
        if (now < s_nextTickMs) return;

        // next allowed tick: base 45 ms ± gaussian noise (≈ one game tick @ 20 TPS
        // with natural human jitter layered on top)
        std::uniform_int_distribution<int> jitterMs(-8, 14);
        s_nextTickMs = now + 45 + (DWORD)jitterMs(s_rng);
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

        jobject playersList = env->GetObjectField(world, playersField);
        if (!playersList) goto cleanup;

        int size = env->CallIntMethod(playersList, listSize);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(playersList); goto cleanup; }

        jobject bestTarget   = nullptr;
        double  bestDist     = 6.0;
        float   bestYawDiff  = fov;

        for (int i = 0; i < size; i++) {
            jobject target = env->CallObjectMethod(playersList, listGet, i);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (!target) continue;

            if (env->IsSameObject(player, target)) { env->DeleteLocalRef(target); continue; }

            double tx = env->GetDoubleField(target, entX);
            double ty = env->GetDoubleField(target, entY);
            double tz = env->GetDoubleField(target, entZ);

            double dist = std::sqrt(std::pow(tx-px,2) + std::pow(ty-py,2) + std::pow(tz-pz,2));

            if (dist <= bestDist) {
                float hp = env->CallFloatMethod(target, getHealth);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(target); continue; }
                if (hp > 0.0f) {
                    float targetYaw = (float)(std::atan2(tz-pz, tx-px) * 180.0 / 3.14159265) - 90.0f;
                    float yawDiff   = wrap180(targetYaw - currentYaw);
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

            double diffX  = tx - px;
            double diffY  = (ty + 1.0) - (py + 1.62);
            double diffZ  = tz - pz;
            double distXZ = std::sqrt(diffX*diffX + diffZ*diffZ);

            float targetYaw   = (float)(std::atan2(diffZ, diffX) * 180.0 / 3.14159265) - 90.0f;
            float targetPitch = (float)-(std::atan2(diffY, distXZ) * 180.0 / 3.14159265);

            float yawDiff   = wrap180(targetYaw   - currentYaw);
            float pitchDiff = wrap180(targetPitch - currentPitch);

            // ── GCD compensation BEFORE lerp (correct order) ─────────────────
            float gcd = 1.0f;
            if (sensitivityField && getDoubleValue) {
                jobject sensObj = env->GetObjectField(options, sensitivityField);
                if (sensObj) {
                    jobject sensDoubleObj = env->CallObjectMethod(sensObj, getDoubleValue);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (sensDoubleObj) {
                        jclass    dblClass  = env->FindClass("java/lang/Double");
                        jmethodID dblValMID = env->GetMethodID(dblClass, "doubleValue", "()D");
                        double sens = env->CallDoubleMethod(sensDoubleObj, dblValMID);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); sens = 0.5; }
                        float f = (float)(sens * 0.6 + 0.2);
                        gcd = f * f * f * 1.2f;
                        env->DeleteLocalRef(dblClass);
                        env->DeleteLocalRef(sensDoubleObj);
                    }
                    env->DeleteLocalRef(sensObj);
                }
            }

            // Snap deltas to GCD grid first, then smooth — prevents sub-GCD
            // remainder accumulation that looks inhuman on server-side analysis
            yawDiff   -= std::fmod(yawDiff,   gcd);
            pitchDiff -= std::fmod(pitchDiff, gcd);

            // ── spring-damper velocity model ─────────────────────────────────
            // Accelerate toward target delta, then coast + decelerate.
            // This produces the characteristic S-curve a real wrist makes.
            float baseSpeed = smoothSpeed;

            // Vary speed based on angular distance: faster when far, slower on
            // approach — mimics natural human over/undershoot behaviour
            float angDist = std::sqrt(yawDiff*yawDiff + pitchDiff*pitchDiff);
            float distScale = (std::min)(1.0f, angDist / 15.0f); // ramp up over 15 deg
            float targetSpeed = baseSpeed * (0.55f + 0.45f * distScale);

            // Add per-tick gaussian noise to the speed itself
            targetSpeed += gaussian_noise(baseSpeed * 0.07f);
            targetSpeed  = (std::max)(0.02f, (std::min)(targetSpeed, 0.95f));

            // Smooth the speed itself (velocity of the velocity = acceleration)
            s_yawVel   += (yawDiff   * targetSpeed - s_yawVel)   * 0.35f;
            s_pitchVel += (pitchDiff * targetSpeed - s_pitchVel) * 0.35f;

            // Small independent noise on each axis (micro-tremor)
            s_yawVel   += gaussian_noise(0.012f);
            s_pitchVel += gaussian_noise(0.008f);

            float newYaw   = currentYaw   + s_yawVel;
            float newPitch = currentPitch + s_pitchVel;

            // Clamp pitch to valid MC range
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
