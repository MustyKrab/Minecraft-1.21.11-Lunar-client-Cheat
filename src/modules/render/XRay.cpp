#include "XRay.h"
#include "../../core/JNIHelper.h"
#include <iostream>

static bool xrayMappingsLoaded = false;
static jclass mcClass, worldRendererClass;
static jfieldID instanceField, worldRendererField;
static jmethodID reloadMethod;

XRay::XRay() : Module("XRay") {}

void XRay::OnEnable() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!xrayMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        worldRendererClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_761;", "net/minecraft/client/render/WorldRenderer");
        
        if (!mcClass || !worldRendererClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        worldRendererField = JNIHelper::GetFieldSafe(mcClass, "field_1769", "Lnet/minecraft/class_761;", "worldRenderer");
        reloadMethod = JNIHelper::GetMethodSafe(worldRendererClass, "method_3279", "()V", "reload");

        xrayMappingsLoaded = true;
    }

    if (!instanceField || !worldRendererField || !reloadMethod) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject worldRenderer = env->GetObjectField(mc, worldRendererField);
    if (worldRenderer) {
        // Trigger a chunk reload to apply X-Ray (requires a mixin or bytecode patch to actually change block visibility)
        // Since we are pure C++, we can't easily hook the block render loop without a bytecode patcher.
        // For now, we just trigger the reload. A full X-Ray requires hooking Block#shouldDrawSide or similar.
        env->CallVoidMethod(worldRenderer, reloadMethod);
        env->DeleteLocalRef(worldRenderer);
    }
    env->DeleteLocalRef(mc);
}

void XRay::OnDisable() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!xrayMappingsLoaded) return;

    jobject mc = env->GetStaticObjectField(mcClass, instanceField);
    if (!mc) return;

    jobject worldRenderer = env->GetObjectField(mc, worldRendererField);
    if (worldRenderer) {
        env->CallVoidMethod(worldRenderer, reloadMethod);
        env->DeleteLocalRef(worldRenderer);
    }
    env->DeleteLocalRef(mc);
}

void XRay::OnTick() {
    // X-Ray logic is handled via block rendering hooks, not per-tick.
}
