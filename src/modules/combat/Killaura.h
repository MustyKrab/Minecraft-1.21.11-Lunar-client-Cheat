#pragma once
#include "../Module.h"

class Killaura : public Module {
private:
    float reach;

public:
    Killaura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }
};
