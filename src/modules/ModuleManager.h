#pragma once
#include <vector>
#include "Module.h"

class ModuleManager {
private:
    static std::vector<Module*> modules;

public:
    static void Initialize();
    static void OnTick();
    static Module* GetModule(const std::string& name);
};
