#include "ModuleManager.h"
#include "combat/Killaura.h"
#include "render/ESP.h"

std::vector<Module*> ModuleManager::modules;

void ModuleManager::Initialize() {
    modules.push_back(new Killaura());
    modules.push_back(new ESP());
    
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
