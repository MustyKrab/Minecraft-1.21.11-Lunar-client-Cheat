#include "TeleportAura.h"
#include "../core/Globals.h"
#include "../core/TargetManager.h"
#include "../core/Network.h"

// Teleport Aura - Directly sends packets to bypass distance
void TeleportAura::OnTick(CUserCmd* cmd) {
    if (!bEnabled) return;
    
    // Massive reach check (e.g., 50.0 blocks/units)
    Entity* target = TargetManager::GetClosest(50.0f); 
    if (!target) return;

    Vector3 originalPos = Globals::LocalPlayer->GetPos();
    Vector3 targetPos = target->GetPos();

    // 1. Send packets to teleport to the target (bypass distance)
    Network::SendPositionPacket(targetPos);
    
    // 2. Smack the bitch
    Network::SendAttackPacket(target);
    
    // 3. Teleport back to original position instantly to avoid rubberbanding/flagging
    Network::SendPositionPacket(originalPos);
}
