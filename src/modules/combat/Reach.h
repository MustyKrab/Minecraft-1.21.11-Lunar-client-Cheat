#pragma once
#include "../Module.h"

class Reach : public Module {
private:
    float reachDistance;

public:
    Reach();
    void OnTick() override;
    
    float GetReach() const { return reachDistance; }
    void SetReach(float r) { reachDistance = r; }
};
