#include "Menu.h"
#include "../modules/KillAura.h"
#include "../modules/TeleportAura.h"
#include <imgui.h>

void Menu::RenderCombatTab() {
    ImGui::BeginChild("Combat", ImVec2(0, 0), true);
    
    // Separate toggles in the UI
    ImGui::Checkbox("KillAura", &Modules::KillAura::bEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Standard melee aura. Keep reach under 3.0f for bypass.");

    ImGui::Checkbox("Teleport Aura", &Modules::TeleportAura::bEnabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Cross-map teleport attacks. Disable KillAura before using.");

    ImGui::EndChild();
}
