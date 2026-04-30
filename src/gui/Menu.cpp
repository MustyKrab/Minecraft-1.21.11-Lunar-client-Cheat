#include "Menu.h"
#include "../modules/combat/Killaura.h"
#include "../modules/combat/Aimbot.h"
#include "../modules/TeleportAura.h"
#include "../modules/render/XRay.h"
#include "../modules/ModuleManager.h"
#include <imgui.h>

void Menu::RenderCombatTab() {
    ImGui::BeginChild("Combat", ImVec2(0, 0), true);

    Killaura* ka = static_cast<Killaura*>(ModuleManager::GetModule("Killaura"));
    if (ka) {
        bool enabled = ka->IsEnabled();
        if (ImGui::Checkbox("KillAura", &enabled))
            ka->SetEnabled(enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Standard melee aura. Keep reach under 3.0f for bypass.");

        float reach = ka->GetReach();
        if (ImGui::SliderFloat("Reach##ka", &reach, 2.0f, 6.0f, "%.1f"))
            ka->SetReach(reach);

        float fov = ka->GetFOV();
        if (ImGui::SliderFloat("FOV##ka", &fov, 10.0f, 180.0f, "%.0f"))
            ka->SetFOV(fov);

        bool enable360 = ka->Is360Enabled();
        if (ImGui::Checkbox("360 Mode##ka", &enable360))
            ka->Set360Enabled(enable360);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bypass FOV check to hit entities all around you.");

        bool pktOpt = ka->IsPacketOrderOptimizeEnabled();
        if (ImGui::Checkbox("Packet Order Optimize##ka", &pktOpt))
            ka->SetPacketOrderOptimizeEnabled(pktOpt);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Attack packet sent before movement packet. Server evaluates reach at last-tick position.");
    }

    ImGui::Separator();

    Aimbot* ab = static_cast<Aimbot*>(ModuleManager::GetModule("Aimbot"));
    if (ab) {
        bool abEnabled = ab->IsEnabled();
        if (ImGui::Checkbox("Aimbot", &abEnabled))
            ab->SetEnabled(abEnabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Smooth humanised aim assist. Activates on LMB hold.");

        float abFov = ab->GetFOV();
        if (ImGui::SliderFloat("FOV##ab", &abFov, 10.0f, 180.0f, "%.0f"))
            ab->SetFOV(abFov);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Angular radius (degrees) to scan for targets. Lower = tighter lock.");

        float abSpeed = ab->GetSmoothSpeed();
        if (ImGui::SliderFloat("Smooth Speed##ab", &abSpeed, 0.01f, 1.0f, "%.2f"))
            ab->SetSmoothSpeed(abSpeed);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Base lerp factor. 0.01 = very slow/human, 1.0 = near-instant.");
    }

    ImGui::EndChild();
}

void Menu::RenderTeleportAuraTab() {
    ImGui::BeginChild("Teleport Aura", ImVec2(0, 0), true);

    TeleportAura* ta = static_cast<TeleportAura*>(ModuleManager::GetModule("TeleportAura"));
    if (ta) {
        bool enabled = ta->IsEnabled();
        if (ImGui::Checkbox("Teleport Aura", &enabled))
            ta->SetEnabled(enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Cross-map teleport attacks. Standalone module.");

        float reach = ta->GetReach();
        if (ImGui::SliderFloat("Reach##ta", &reach, 10.0f, 100.0f, "%.0f"))
            ta->SetReach(reach);
    }

    ImGui::EndChild();
}

void Menu::RenderVisualsTab() {
    ImGui::BeginChild("Visuals", ImVec2(0, 0), true);

    XRay* xray = static_cast<XRay*>(ModuleManager::GetModule("XRay"));
    if (xray) {
        bool enabled = xray->IsEnabled();
        if (ImGui::Checkbox("XRay", &enabled))
            xray->SetEnabled(enabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scans nearby blocks and highlights ores/chests.");

        ImGui::Indent();
        ImGui::Checkbox("Diamond",     &xray->showDiamond);
        ImGui::Checkbox("Gold",        &xray->showGold);
        ImGui::Checkbox("Iron",        &xray->showIron);
        ImGui::Checkbox("Emerald",     &xray->showEmerald);
        ImGui::Checkbox("Netherite",   &xray->showNetherite);
        ImGui::Checkbox("Chests",      &xray->showChests);
        ImGui::Checkbox("Ender Chests",&xray->showEnderChests);
        ImGui::Checkbox("Spawners",    &xray->showSpawners);
        ImGui::Checkbox("Hoppers",     &xray->showHoppers);
        ImGui::Unindent();
    }

    ImGui::EndChild();
}
