#include "XRay.h"
#include "../core/SDK.h"

bool Modules::XRay::bEnabled = false;
bool Modules::XRay::bNeedsUpdate = false;

void Modules::XRay::Toggle() {
    bEnabled = !bEnabled;
    bNeedsUpdate = true; // Queue the update instead of calling it directly
}

// Call this from a safe main-thread hook (e.g., ClientTick)
void Modules::XRay::ProcessSafeUpdate(JNIEnv* env) {
    if (!bNeedsUpdate) return;

    // Safely grab the Minecraft instance
    jobject mc = SDK::Minecraft::GetInstance(env);
    if (!mc) {
        bNeedsUpdate = false;
        return;
    }

    // Grab WorldRenderer / RenderGlobal
    jobject worldRenderer = SDK::Minecraft::GetWorldRenderer(env, mc);
    if (worldRenderer) {
        // Safely trigger the chunk mesh rebuild on the correct thread
        SDK::WorldRenderer::ReloadChunks(env, worldRenderer);
    }

    bNeedsUpdate = false;
}
