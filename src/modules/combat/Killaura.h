#pragma once
#include "../Module.h"

class Killaura : public Module {
private:
    float reach;
    float fov;
    bool enable360;
    int ticksSinceLastAttack;
    int randomDelay;

public:
    Killaura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }
    
    float GetFOV() const { return fov; }
    void SetFOV(float f) { fov = f; }

    bool Is360Enabled() const { return enable360; }
    void Set360Enabled(bool e) { enable360 = e; }
};
