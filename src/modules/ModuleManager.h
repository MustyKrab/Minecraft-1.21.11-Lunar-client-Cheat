#pragma once
#include <vector>
#include <string>
#include "Module.h"

class ModuleManager {
public:
    static std::vector<Module*> modules;

    static void Initialize();
    static void Cleanup();
    static void OnTick();
    static Module* GetModule(const std::string& name);
    static const std::vector<Module*>& GetModules() { return modules; }
};
