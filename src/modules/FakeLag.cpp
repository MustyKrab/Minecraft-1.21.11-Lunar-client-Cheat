#include "FakeLag.h"
#include "../core/JNIHelper.h"
#include <iostream>

static bool flMappingsLoaded = false;
static jclass flMcClass, flPlayerClass, flNetworkHandlerClass;
static jfieldID flInstanceField, flPlayerField, flNetworkHandlerField;
static jmethodID flSendPacketMethod;

FakeLag::FakeLag() : Module("FakeLag"), chokeLimit(10), chokedPackets(0) {}

void FakeLag::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!flMappingsLoaded) {
        flMcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        flPlayerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_746;", "net/minecraft/client/network/ClientPlayerEntity");
        flNetworkHandlerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_634;", "net/minecraft/client/network/ClientPlayNetworkHandler");

        if (!flMcClass || !flPlayerClass || !flNetworkHandlerClass) return;

        flInstanceField = JNIHelper::GetStaticFieldSafe(flMcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        flPlayerField = JNIHelper::GetFieldSafe(flMcClass, "field_1724", "Lnet/minecraft/class_746;", "player");
        flNetworkHandlerField = JNIHelper::GetFieldSafe(flPlayerClass, "field_3944", "Lnet/minecraft/class_634;", "networkHandler");
        flSendPacketMethod = JNIHelper::GetMethodSafe(flNetworkHandlerClass, "method_52787", "(Lnet/minecraft/class_2596;)V", "sendPacket");

        // Only mark loaded when ALL fields resolved
        if (!flInstanceField || !flPlayerField || !flNetworkHandlerField || !flSendPacketMethod) return;

        flMappingsLoaded = true;
    }

    if (!flInstanceField || !flPlayerField || !flNetworkHandlerField || !flSendPacketMethod) return;

    jobject mc = env->GetStaticObjectField(flMcClass, flInstanceField);
    if (!mc) return;

    jobject player = env->GetObjectField(mc, flPlayerField);
    if (!player) {
        env->DeleteLocalRef(mc);
        return;
    }

    // Real FakeLag requires hooking sendPacket via MinHook or a Java agent to queue packets.
    // JNI-only approach can't intercept outgoing packets without bytecode instrumentation.
    // This counter tracks choke state for GUI display purposes.
    chokedPackets++;
    if (chokedPackets >= chokeLimit) {
        chokedPackets = 0;
    }

    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mc);
}
