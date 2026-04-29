#include "KillAura.h"
#include "../utils/Math.h"
#include "../core/Globals.h"
#include "../core/TargetManager.h"

// Standard KillAura - Modifies CUserCmd for normal attacks
void KillAura::OnTick(CUserCmd* cmd) {
    if (!bEnabled) return;
    
    // Standard reach check (e.g., 3.0 blocks/units)
    Entity* target = TargetManager::GetClosest(3.0f); 
    if (!target) return;

    // Normal rotation calculation
    Math::CalcAngle(Globals::LocalPlayer->GetEyePos(), target->GetBonePos(8), cmd->viewangles);
    
    // Attack
    cmd->buttons |= IN_ATTACK;
}
