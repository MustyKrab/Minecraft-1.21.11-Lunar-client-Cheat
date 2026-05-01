#include "XRay.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cstring>
#include <cmath>

static bool xrayMappingsLoaded = false;
static int  retryCounter       = 0;

static jclass     mcClass, worldClass, blockPosClass, blockStateClass, blockClass;
static jfieldID   instanceField, playerField, worldField;
static jfieldID   entX, entY, entZ;
static jmethodID  getBlockStateMethod, getBlockMethod, toStringMethod;
// Moved out of scan loop — resolved once during mapping load
static jmethodID  blockPosInit;

XRay::XRay() : Module("XRay") {
    // Clamp stored radius to sane default; callers may still set it higher
    // but 16 keeps the per-scan JNI call count under ~35k
    scanRadius = 16;
}

void XRay::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    // ─ Mapping load (retried every 60 ticks until all IDs resolve) ─────────
    if (!xrayMappingsLoaded) {
        retryCounter++;
        if (retryCounter < 60) return;
        retryCounter = 0;

        mcClass         = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",   "net/minecraft/client/MinecraftClient");
        worldClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",   "net/minecraft/client/world/ClientWorld");
        blockPosClass   = JNIHelper::FindClassSafe("Lnet/minecraft/class_2338;",  "net/minecraft/util/math/BlockPos");
        blockStateClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2680;",  "net/minecraft/block/BlockState");
        blockClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_2248;",  "net/minecraft/block/Block");

        if (!mcClass || !worldClass || !blockPosClass || !blockStateClass || !blockClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField   = JNIHelper::GetFieldSafe(mcClass,       "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField    = JNIHelper::GetFieldSafe(mcClass,       "field_1687", "Lnet/minecraft/class_638;", "world");

        jclass entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        getBlockStateMethod = JNIHelper::GetMethodSafe(worldClass,      "method_8320",  "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2680;", "getBlockState");
        getBlockMethod      = JNIHelper::GetMethodSafe(blockStateClass, "method_26204", "()Lnet/minecraft/class_2248;",                           "getBlock");

        // BlockPos constructor — resolved here, reused every scan
        blockPosInit = env->GetMethodID(blockPosClass, "<init>", "(III)V");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

        // FIX: use getTranslationKey instead of toString.
        // method_9518 gets the translation key (e.g. "block.minecraft.diamond_ore")
        toStringMethod = JNIHelper::GetMethodSafe(blockClass, "method_9518", "()Ljava/lang/String;", "getTranslationKey");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

        if (!instanceField || !worldField || !getBlockStateMethod || !getBlockMethod ||
            !toStringMethod || !blockPosInit)
            return;

        xrayMappingsLoaded = true;
    }

    if (!instanceField || !worldField || !getBlockStateMethod || !getBlockMethod ||
        !toStringMethod || !blockPosInit)
        return;

    // ─ Tick-rate gate (every 40 ticks = 2 s @ 20 TPS) ──────────
    tickCounter++;
    if (tickCounter < 40) return;
    tickCounter = 0;

    // ─ Fetch MC instance, player, world ────────────────────────
    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    jobject world  = env->GetObjectField(mc, worldField);

    if (!player || !world) {
        if (player) env->DeleteLocalRef(player);
        if (world)  env->DeleteLocalRef(world);
        env->DeleteLocalRef(mc);
        return;
    }

    int px = (int)env->GetDoubleField(player, entX);
    int py = (int)env->GetDoubleField(player, entY);
    int pz = (int)env->GetDoubleField(player, entZ);

    // ─ Position-delta rescan guard ─────────────────────────────
    // If we haven't moved much AND we just toggled a setting, we still need to rescan.
    // We'll track the last settings state to force a rescan if it changed.
    static bool lastShowDiamond, lastShowGold, lastShowIron, lastShowEmerald, lastShowNetherite;
    static bool lastShowChests, lastShowEnderChests, lastShowSpawners, lastShowHoppers;
    static int  lastScanRadius = -1;

    bool settingsChanged = (
        showDiamond != lastShowDiamond || showGold != lastShowGold ||
        showIron != lastShowIron || showEmerald != lastShowEmerald ||
        showNetherite != lastShowNetherite || showChests != lastShowChests ||
        showEnderChests != lastShowEnderChests || showSpawners != lastShowSpawners ||
        showHoppers != lastShowHoppers || scanRadius != lastScanRadius
    );

    double distMoved = std::sqrt(
        std::pow(px - lastScanX, 2) +
        std::pow(py - lastScanY, 2) +
        std::pow(pz - lastScanZ, 2)
    );

    // Only skip if we haven't moved enough AND settings haven't changed
    if (distMoved < 8.0 && !settingsChanged) {
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

    lastScanX = px;
    lastScanY = py;
    lastScanZ = pz;
    
    lastShowDiamond = showDiamond;
    lastShowGold = showGold;
    lastShowIron = showIron;
    lastShowEmerald = showEmerald;
    lastShowNetherite = showNetherite;
    lastShowChests = showChests;
    lastShowEnderChests = showEnderChests;
    lastShowSpawners = showSpawners;
    lastShowHoppers = showHoppers;
    lastScanRadius = scanRadius;

    std::vector<XRayBlock> newFoundBlocks;
    newFoundBlocks.reserve(256);

    // ─ Scan loop — PushLocalFrame guarantees cleanup on any exit path ─
    int radius = scanRadius;

    for (int x = px - radius; x <= px + radius; x++) {
        for (int y = py - radius; y <= py + radius; y++) {
            if (y < -64 || y > 320) continue;
            for (int z = pz - radius; z <= pz + radius; z++) {

                // Each inner iteration gets its own local frame (4 refs: pos, state, block, str)
                if (env->PushLocalFrame(8) < 0) continue;

                jobject posObj = env->NewObject(blockPosClass, blockPosInit, x, y, z);
                if (!posObj) { env->PopLocalFrame(nullptr); continue; }

                jobject stateObj = env->CallObjectMethod(world, getBlockStateMethod, posObj);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->PopLocalFrame(nullptr); continue; }
                if (!stateObj)             { env->PopLocalFrame(nullptr); continue; }

                jobject blockObj = env->CallObjectMethod(stateObj, getBlockMethod);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->PopLocalFrame(nullptr); continue; }
                if (!blockObj)             { env->PopLocalFrame(nullptr); continue; }

                jstring keyStr = (jstring)env->CallObjectMethod(blockObj, toStringMethod);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->PopLocalFrame(nullptr); continue; }

                if (keyStr) {
                    const char* rawKey = env->GetStringUTFChars(keyStr, nullptr);
                    if (rawKey) {
                        // ─ Ores ──────────────────────────────────────────
                        if      (showDiamond   && strstr(rawKey, "diamond_ore"))    { newFoundBlocks.push_back({x, y, z,   0, 255, 255}); }
                        else if (showGold      && strstr(rawKey, "gold_ore"))       { newFoundBlocks.push_back({x, y, z, 255, 215,   0}); }
                        else if (showIron      && strstr(rawKey, "iron_ore"))       { newFoundBlocks.push_back({x, y, z, 200, 200, 200}); }
                        else if (showEmerald   && strstr(rawKey, "emerald_ore"))    { newFoundBlocks.push_back({x, y, z,   0, 255,   0}); }
                        else if (showNetherite && strstr(rawKey, "ancient_debris")) { newFoundBlocks.push_back({x, y, z, 100,  70,  70}); }
                        // ─ Containers (order matters: specific before generic) ─
                        else if (showEnderChests && strstr(rawKey, "ender_chest"))    { newFoundBlocks.push_back({x, y, z, 128,   0, 128}); }
                        else if (showChests      && strstr(rawKey, "trapped_chest"))  { newFoundBlocks.push_back({x, y, z, 255, 100,   0}); }
                        else if (showChests      && (strstr(rawKey, "chest") ||
                                                     strstr(rawKey, "barrel") ||
                                                     strstr(rawKey, "shulker_box")))  { newFoundBlocks.push_back({x, y, z, 255, 165,   0}); }
                        // ─ Misc ──────────────────────────────────────────
                        else if (showSpawners && strstr(rawKey, "spawner"))        { newFoundBlocks.push_back({x, y, z, 255,   0,   0}); }
                        else if (showHoppers  && strstr(rawKey, "hopper"))         { newFoundBlocks.push_back({x, y, z, 100, 100, 100}); }

                        env->ReleaseStringUTFChars(keyStr, rawKey);
                    }
                }

                // PopLocalFrame releases posObj, stateObj, blockObj, keyStr in one shot
                env->PopLocalFrame(nullptr);
            }
        }
    }

    SetFoundBlocks(newFoundBlocks);

    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
