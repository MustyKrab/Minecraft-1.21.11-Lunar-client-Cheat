#include "Menu.h"
#include "../modules/combat/Killaura.h"
#include "../modules/TeleportAura.h"
#include "../modules/render/XRay.h"
#include "../modules/ModuleManager.h"
#include <imgui.h>

void Menu::RenderCombatTab() {
    ImGui::BeginChild("Combat", ImVec2(0, 0), true);
    
    // Get the Killaura module instance from ModuleManager
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
        if (ImGui::Checkbox("XRay", &enabled)) {
            xray->SetEnabled(enabled);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scans nearby blocks and highlights ores/chests.");

        ImGui::Indent();
        ImGui::Checkbox("Diamond",      &xray->showDiamond);
        ImGui::Checkbox("Gold",         &xray->showGold);
        ImGui::Checkbox("Iron",         &xray->showIron);
        ImGui::Checkbox("Emerald",      &xray->showEmerald);
        ImGui::Checkbox("Netherite",    &xray->showNetherite);
        ImGui::Checkbox("Chests",       &xray->showChests);
        ImGui::Checkbox("Ender Chests", &xray->showEnderChests);
        ImGui::Checkbox("Spawners",     &xray->showSpawners);
        ImGui::Checkbox("Hoppers",      &xray->showHoppers);
        ImGui::Unindent();
    }

    ImGui::EndChild();
}
