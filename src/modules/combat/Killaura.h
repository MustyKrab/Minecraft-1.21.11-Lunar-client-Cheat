#pragma once
#include "../Module.h"

class Killaura : public Module {
private:
    float reach;
    float fov;

public:
    Killaura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }
    
    float GetFOV() const { return fov; }
    void SetFOV(float f) { fov = f; }
};