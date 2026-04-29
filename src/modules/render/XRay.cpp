#include "XRay.h"
#include "../../core/JNIHelper.h"
#include <iostream>
#include <cstring>

static bool xrayMappingsLoaded = false;
static jclass mcClass, worldClass, blockPosClass, blockStateClass, blockClass, registryClass;
static jfieldID instanceField, playerField, worldField;
static jfieldID entX, entY, entZ;
static jmethodID getBlockStateMethod, getBlockMethod, getTranslationKeyMethod;

XRay::XRay() : Module("XRay") {}

void XRay::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!xrayMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        worldClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_638;", "net/minecraft/client/world/ClientWorld");
        blockPosClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2338;", "net/minecraft/util/math/BlockPos");
        blockStateClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2680;", "net/minecraft/block/BlockState");
        blockClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2248;", "net/minecraft/block/Block");
        registryClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_2378;", "net/minecraft/registry/Registry");

        if (!mcClass || !worldClass || !blockPosClass || !blockStateClass || !blockClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        playerField = JNIHelper::GetFieldSafe(mcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        worldField = JNIHelper::GetFieldSafe(mcClass, "field_1687", "Lnet/minecraft/class_638;", "world");

        jclass entityClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_1297;", "net/minecraft/entity/Entity");
        entX = JNIHelper::GetFieldSafe(entityClass, "field_6014", "D", "x");
        entY = JNIHelper::GetFieldSafe(entityClass, "field_6036", "D", "y");
        entZ = JNIHelper::GetFieldSafe(entityClass, "field_5969", "D", "z");

        getBlockStateMethod = JNIHelper::GetMethodSafe(worldClass, "method_8320", "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2680;", "getBlockState");
        getBlockMethod = JNIHelper::GetMethodSafe(blockStateClass, "method_26204", "()Lnet/minecraft/class_2248;", "getBlock");
        getTranslationKeyMethod = JNIHelper::GetMethodSafe(blockClass, "method_9539", "()Ljava/lang/String;", "getTranslationKey");

        xrayMappingsLoaded = true;
    }

    if (!instanceField || !worldField || !getBlockStateMethod) return;

    // Only scan every 20 ticks (1 second) to prevent massive lag
    tickCounter++;
    if (tickCounter < 20) return;
    tickCounter = 0;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, playerField);
    jobject world = env->GetObjectField(mc, worldField);

    if (!player || !world) {
        if (player) env->DeleteLocalRef(player);
        if (world) env->DeleteLocalRef(world);
        env->DeleteLocalRef(mc);
        return;
    }

    int px = (int)env->GetDoubleField(player, entX);
    int py = (int)env->GetDoubleField(player, entY);
    int pz = (int)env->GetDoubleField(player, entZ);

    std::vector<BlockPos> newFoundBlocks;

    jmethodID blockPosInit = env->GetMethodID(blockPosClass, "<init>", "(III)V");

    // Scan a 32x32x32 area around the player
    for (int x = px - scanRadius; x <= px + scanRadius; x++) {
        for (int y = py - scanRadius; y <= py + scanRadius; y++) {
            // Don't scan above ground much or below bedrock
            if (y < -64 || y > 100) continue; 
            
            for (int z = pz - scanRadius; z <= pz + scanRadius; z++) {
                jobject posObj = env->NewObject(blockPosClass, blockPosInit, x, y, z);
                jobject stateObj = env->CallObjectMethod(world, getBlockStateMethod, posObj);
                
                if (stateObj) {
                    jobject blockObj = env->CallObjectMethod(stateObj, getBlockMethod);
                    if (blockObj) {
                        jstring keyStr = (jstring)env->CallObjectMethod(blockObj, getTranslationKeyMethod);
                        if (keyStr) {
                            const char* rawKey = env->GetStringUTFChars(keyStr, nullptr);
                            
                            // Check for Diamond Ore or Deepslate Diamond Ore
                            if (strstr(rawKey, "diamond_ore") != nullptr) {
                                newFoundBlocks.push_back({x, y, z});
                            }
                            
                            env->ReleaseStringUTFChars(keyStr, rawKey);
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

    foundBlocks = newFoundBlocks;

    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
