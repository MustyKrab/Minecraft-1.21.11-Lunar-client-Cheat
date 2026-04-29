#pragma once
#include <string>

class Module {
protected:
    std::string name;
    bool enabled;
    bool expanded;

public:
    Module(std::string name) : name(name), enabled(false), expanded(false) {}
    virtual ~Module() = default;

    std::string GetName() { return name; }
    
    bool IsEnabled() { return enabled; }
    void SetEnabled(bool e) { enabled = e; }
    
    bool IsExpanded() { return expanded; }
    void SetExpanded(bool e) { expanded = e; }
    
    void Toggle() {
        enabled = !enabled;
        if (enabled) OnEnable();
        else OnDisable();
    }

    virtual void OnEnable() {}
    virtual void OnDisable() {}
    virtual void OnTick() {}
};
