#include "ModuleManager.h"
#include "combat/Killaura.h"
#include "combat/Aimbot.h"
#include "combat/Reach.h"
#include "combat/WTap.h"
#include "combat/AutoClicker.h"
#include "TeleportAura.h"
#include "render/ESP.h"
#include "render/XRay.h"
#include "player/Fly.h"

std::vector<Module*> ModuleManager::modules;

void ModuleManager::Initialize() {
    modules.push_back(new Killaura());
    modules.push_back(new Aimbot());
    modules.push_back(new AutoClicker());
    modules.push_back(new Reach());
    modules.push_back(new WTap());
    modules.push_back(new TeleportAura());
    modules.push_back(new ESP());
    modules.push_back(new XRay());
    modules.push_back(new Fly());
    
    // Enable ESP by default for testing
    GetModule("ESP")->Toggle();
}

void ModuleManager::OnTick() {
    for (Module* mod : modules) {
        if (mod->IsEnabled()) {
            mod->OnTick();
        }
    }
}

Module* ModuleManager::GetModule(const std::string& name) {
    for (Module* mod : modules) {
        if (mod->GetName() == name) {
            return mod;
        }
    }
    return nullptr;
}