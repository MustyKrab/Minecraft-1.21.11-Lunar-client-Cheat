#pragma once
#include <string>

class Module {
protected:
    std::string name;
    bool enabled;

public:
    Module(std::string name) : name(name), enabled(false) {}
    virtual ~Module() = default;

    std::string GetName() { return name; }
    bool IsEnabled() { return enabled; }
    
    void Toggle() {
        enabled = !enabled;
        if (enabled) OnEnable();
        else OnDisable();
    }

    virtual void OnEnable() {}
    virtual void OnDisable() {}
    virtual void OnTick() {}
};
