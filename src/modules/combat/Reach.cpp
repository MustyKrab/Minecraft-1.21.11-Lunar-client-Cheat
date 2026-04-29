#include "Reach.h"
#include "../../core/JNIHelper.h"
#include <iostream>

static bool reachMappingsLoaded = false;
static jclass mcClass, interactionManagerClass;
static jfieldID instanceField, interactionManagerField;
static jmethodID getReachMethod;

Reach::Reach() : Module("Reach"), reachDistance(3.5f) {}

void Reach::OnTick() {
    JNIEnv* env = JNIHelper::env;
    if (!env) return;

    if (!reachMappingsLoaded) {
        mcClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_310;", "net/minecraft/client/MinecraftClient");
        interactionManagerClass = JNIHelper::FindClassSafe("Lnet/minecraft/class_636;", "net/minecraft/client/network/ClientPlayerInteractionManager");

        if (!mcClass || !interactionManagerClass) return;

        instanceField = JNIHelper::GetStaticFieldSafe(mcClass, "field_1700", "Lnet/minecraft/class_310;", "instance");
        interactionManagerField = JNIHelper::GetFieldSafe(mcClass, "field_1761", "Lnet/minecraft/class_636;", "interactionManager");
        
        // In vanilla, reach is hardcoded or handled via attributes in 1.21.
        // For a pure external C++ cheat, modifying the actual reach attribute or hit result is complex without bytecode manipulation.
        // A common external approach is to just use Killaura logic when clicking, or spoof the hit vector.
        // For this implementation, we will rely on the Killaura module for extended reach attacks, as modifying the vanilla reach attribute externally is unreliable.
        
        reachMappingsLoaded = true;
    }
}
