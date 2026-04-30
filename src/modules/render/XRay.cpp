#include "XRay.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cstring>
#include <cmath>

static bool xrayMappingsLoaded = false;
static int retryCounter = 0;
static jclass mcClass, worldClass, blockPosClass, blockStateClass, blockClass;
static jfieldID instanceField, playerField, worldField;
static jfieldID entX, entY, entZ;
static jmethodID getBlockStateMethod, getBlockMethod, toStringMethod;

XRay::XRay() : Module("XRay") {}

void XRay::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!xrayMappingsLoaded) {
        retryCounter++;
        if (retryCounter < 60) return;
        retryCounter = 0;

        mcClass         = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;",  "net/minecraft/client/MinecraftClient");
        worldClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;",  "net/minecraft/client/world/ClientWorld");
        blockPosClass   = JNIHelper::FindClassSafe("Lnet/minecraft/class_2338;", "net/minecraft/util/math/BlockPos");
        blockStateClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2680;", "net/minecraft/block/BlockState");
        blockClass      = JNIHelper::FindClassSafe("Lnet/minecraft/class_2248;", "net/minecraft/block/Block");

        if (!mcClass || !worldClass || !blockPosClass || !blockStateClass || !blockClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField   = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField    = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");

        jclass entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        getBlockStateMethod = JNIHelper::GetMethodSafe(worldClass,      "method_8320",  "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2680;", "getBlockState");
        getBlockMethod      = JNIHelper::GetMethodSafe(blockStateClass, "method_26204", "()Lnet/minecraft/class_2248;",                           "getBlock");

        jclass objectClass = env->FindClass("java/lang/Object");
        if (objectClass) {
            toStringMethod = env->GetMethodID(objectClass, "toString", "()Ljava/lang/String;");
            env->DeleteLocalRef(objectClass);
        }

        // Only mark loaded when ALL critical fields resolved
        if (!instanceField || !worldField || !getBlockStateMethod || !getBlockMethod || !toStringMethod) return;

        xrayMappingsLoaded = true;
    }

    if (!instanceField || !worldField || !getBlockStateMethod || !getBlockMethod || !toStringMethod) return;

    tickCounter++;
    if (tickCounter < 40) return;
    tickCounter = 0;

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

    double distMoved = std::sqrt(std::pow(px - lastScanX, 2) + std::pow(py - lastScanY, 2) + std::pow(pz - lastScanZ, 2));
    if (distMoved < 8.0) {
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

    lastScanX = px;
    lastScanY = py;
    lastScanZ = pz;

    std::vector<XRayBlock> newFoundBlocks;

    jmethodID blockPosInit = env->GetMethodID(blockPosClass, "<init>", "(III)V");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
        return;
    }

    // Use the member scanRadius instead of hardcoded 10
    int radius = scanRadius;
    for (int x = px - radius; x <= px + radius; x++) {
        for (int y = py - radius; y <= py + radius; y++) {
            if (y < -64 || y > 320) continue;
            for (int z = pz - radius; z <= pz + radius; z++) {
                jobject posObj = env->NewObject(blockPosClass, blockPosInit, x, y, z);
                if (!posObj) continue;

                jobject stateObj = env->CallObjectMethod(world, getBlockStateMethod, posObj);
                if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(posObj); continue; }

                if (stateObj) {
                    jobject blockObj = env->CallObjectMethod(stateObj, getBlockMethod);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(stateObj); env->DeleteLocalRef(posObj); continue; }

                    if (blockObj) {
                        jstring keyStr = (jstring)env->CallObjectMethod(blockObj, toStringMethod);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(blockObj); env->DeleteLocalRef(stateObj); env->DeleteLocalRef(posObj); continue; }

                        if (keyStr) {
                            const char* rawKey = env->GetStringUTFChars(keyStr, nullptr);
                            if (rawKey) {
                                if      (strstr(rawKey, "diamond_ore"))   { if (showDiamond)      newFoundBlocks.push_back({x, y, z,   0, 255, 255}); }
                                else if (strstr(rawKey, "gold_ore"))      { if (showGold)         newFoundBlocks.push_back({x, y, z, 255, 215,   0}); }
                                else if (strstr(rawKey, "iron_ore"))      { if (showIron)         newFoundBlocks.push_back({x, y, z, 200, 200, 200}); }
                                else if (strstr(rawKey, "emerald_ore"))   { if (showEmerald)      newFoundBlocks.push_back({x, y, z,   0, 255,   0}); }
                                else if (strstr(rawKey, "ancient_debris")){ if (showNetherite)    newFoundBlocks.push_back({x, y, z, 100,  70,  70}); }
                                else if (strstr(rawKey, "ender_chest"))   { if (showEnderChests)  newFoundBlocks.push_back({x, y, z, 128,   0, 128}); }
                                else if (strstr(rawKey, "chest") ||
                                         strstr(rawKey, "barrel"))        { if (showChests)       newFoundBlocks.push_back({x, y, z, 255, 165,   0}); }
                                else if (strstr(rawKey, "spawner"))       { if (showSpawners)     newFoundBlocks.push_back({x, y, z, 255,   0,   0}); }
                                else if (strstr(rawKey, "hopper"))        { if (showHoppers)      newFoundBlocks.push_back({x, y, z, 100, 100, 100}); }
                                env->ReleaseStringUTFChars(keyStr, rawKey);
                            }
                            env->DeleteLocalRef(keyStr);
                        }
                        env->DeleteLocalRef(blockObj);
                    }
                    env->DeleteLocalRef(stateObj);
                }
                env->DeleteLocalRef(posObj);
            }
        }
    }

    SetFoundBlocks(newFoundBlocks);

    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
