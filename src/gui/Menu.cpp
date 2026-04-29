#include "Menu.h"
#include "../modules/combat/Killaura.h"
#include "../modules/TeleportAura.h"
#include "../modules/render/XRay.h"
#include <imgui.h>

void Menu::RenderCombatTab() {
    ImGui::BeginChild("Combat", ImVec2(0, 0), true);
    
    // Killaura is now standalone
    ImGui::Checkbox("KillAura", &Modules::KillAura::bEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Standard melee aura. Keep reach under 3.0f for bypass.");

    ImGui::EndChild();
}

void Menu::RenderTeleportAuraTab() {
    ImGui::BeginChild("Teleport Aura", ImVec2(0, 0), true);
    
    ImGui::Checkbox("Teleport Aura", &Modules::TeleportAura::bEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Cross-map teleport attacks. Standalone module.");

    ImGui::EndChild();
}

void Menu::RenderVisualsTab() {
    ImGui::BeginChild("Visuals", ImVec2(0, 0), true);
    
    bool xrayState = Modules::XRay::bEnabled;
    if (ImGui::Checkbox("XRay", &xrayState)) {
        Modules::XRay::Toggle(); // Safely queues the update
    }

    ImGui::EndChild();
}