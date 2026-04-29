#include "FakeLag.h"
#include "../utils/Math.h"

// FakeLag & Desync - CreateMove Hook
bool __stdcall hkCreateMove(float flInputSampleTime, CUserCmd* cmd) {
    // Call original to populate cmd
    bool result = oCreateMove(flInputSampleTime, cmd);

    if (!cmd || !cmd->command_number)
        return result;

    // Rip bSendPacket from the stack frame
    // Note: Offset depends on the game build. 0x1B is common for CS:GO.
    uintptr_t* ebp;
    __asm mov ebp, ebp;
    bool* bSendPacket = (bool*)(*ebp - 0x1B);

    static int choked_packets = 0;
    const int MAX_CHOKE = 14; // Server forces update at 15

    // 1. Fake Lag Logic
    if (choked_packets < MAX_CHOKE) {
        *bSendPacket = false;
        choked_packets++;
    } else {
        *bSendPacket = true;
        choked_packets = 0;
    }

    // 2. Desync Logic (Anti-Aim)
    // We manipulate cmd->viewangles based on whether we are sending or choking
    if (*bSendPacket) {
        // Real angle: What the server uses for our actual hitbox
        // Keep it aimed at the enemy or our actual view
        cmd->viewangles.y += 0.0f; 
    } else {
        // Fake angle: What the server interpolates during choked ticks
        // Break LBY (Lower Body Yaw) by forcing a massive delta
        cmd->viewangles.y += 120.0f; 
    }

    // Clamp and normalize to prevent Untrusted bans
    Math::NormalizeAngles(cmd->viewangles);
    Math::ClampAngles(cmd->viewangles);

    // Return false so the engine doesn't override our modified angles with SetViewAngles
    return false;
}
