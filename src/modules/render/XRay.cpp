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

    std::vector<XRayBlock> newFoundBlocks;

    jmethodID blockPosInit = env->GetMethodID(blockPosClass, "<init>", "(III)V");

    env->PushLocalFrame(200); // <-- FIX: Prevent JNI local reference leaks

    // Scan a 32x32x32 area around the player
    for (int x = px - scanRadius; x <= px + scanRadius; x++) {
        for (int y = py - scanRadius; y <= py + scanRadius; y++) {
            if (y < -64 || y > 320) continue; 
            
            for (int z = pz - scanRadius; z <= pz + scanRadius; z++) {
                jobject posObj = env->NewObject(blockPosClass, blockPosInit, x, y, z);
                jobject stateObj = env->CallObjectMethod(world, getBlockStateMethod, posObj);
                
                if (stateObj) {
                    jobject blockObj = env->CallObjectMethod(stateObj, getBlockMethod);
                    if (blockObj) {
                        jstring keyStr = (jstring(env->CallObjectMethod(blockObj, getTranslationKeyMethod));
                        if (keyStr) {
                            const char* rawKey = env->GetStringUTFChars(keyStr, nullptr);
                            
                            if (strstr(rawKey, "diamond_ore")) {
                                if (showDiamond) newFoundBlocks.push_back({x, y, z, 0, 255, 255}); // Cyan
                            } else if (strstr(rawKey, "gold_ore")) {
                                if (showGold) newFoundBlocks.push_back({x, y, z, 255, 215, 0}); // Gold
                            } else if (strstr(rawKey, "iron_ore")) {
                                if (showIron)¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÈÀÀ°€ÈÀÀ°€ÈÀÁô¤ì€¼¼M¥±Ù•È(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰•µ•É…±‘}½É”ˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İµ•É…±¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€À°€ÈÔÔ°€Áô¤ì€¼¼É••¸(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰…¹¥•¹Ñ}‘•‰É¥Ìˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İ9•Ñ¡•É¥Ñ”¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÄÀÀ°€ÜÀ°€ÜÁô¤ì€¼¼…É¬	É½İ¸(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰•¹‘•É}¡•ÍĞˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İ¹‘•É¡•ÍÑÌ¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÄÈà°€À°€ÄÈáô¤ì€¼¼AÕÉÁ±”(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰¡•ÍĞˆ¤ñğÍÑÉÍÑÈ¡É…İ-•ä°€‰‰…ÉÉ•°ˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İ¡•ÍÑÌ¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÈÔÔ°€ÄØÔ°€Áô¤ì€¼¼=É…¹”(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰ÍÁ…İ¹•Èˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İMÁ…İ¹•ÉÌ¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÈÔÔ°€À°€Áô¤ì€¼¼I•(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô•±Í”¥˜€¡ÍÑÉÍÑÈ¡É…İ-•ä°€‰¡½ÁÁ•Èˆ¤¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€¥˜€¡Í¡½İ!½ÁÁ•ÉÌ¤¹•İ½Õ¹‘	±½­Ì¹ÁÕÍ¡}‰…¬¡íà°ä°è°€ÄÀÀ°€ÄÀÀ°€ÄÀÁô¤ì€¼¼É…ä(€€€€€€€€€€€€€€€€€€€€€€€€€€€ô(€€€€€€€€€€€€€€€€€€€€€€€€€€€€(€€€€€€€€€€€€€€€€€€€€€€€€€€€•¹Ø´ùI•±•…Í•MÑÉ¥¹UQ¡…ÉÌ¡­•åMÑÈ°É…İ-•ä¤ì(€€€€€€€€€€€€€€€€€€€€€€€€€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡­•åMÑÈ¤ì(€€€€€€€€€€€€€€€€€€€€€€€ô(€€€€€€€€€€€€€€€€€€€€€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡‰±½­=‰¨¤ì(€€€€€€€€€€€€€€€€€€€ô(€€€€€€€€€€€€€€€€€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡ÍÑ…Ñ•=‰¨¤ì(€€€€€€€€€€€€€€€ô(€€€€€€€€€€€€€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡Á½Í=‰¨¤ì(€€€€€€€€€€€ô(€€€€€€€ô(€€€ô((€€€•¹Ø´ùA½Á1½…±É…µ”¡¹Õ±±ÁÑÈ¤ì((€€€ì(€€€€€€€ÍÑèé±½­}Õ…ÉñÍÑèéµÕÑ•àø±½¬¡‰±½­Í5ÕÑ•à¤ì€¼¼€ğ´´%`èQ¡É•…Í…™•Ñä(€€€€€€€™½Õ¹‘	±½­Ì€ô¹•İ½Õ¹‘	±½­Ìì(€€€ô((€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡İ½É±¤ì(€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡Á±…å•È¤ì(€€€•¹Ø´ù•±•Ñ•1½…±I•˜¡µŒ¤ì)ô