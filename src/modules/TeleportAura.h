#pragma once
#include "Module.h"

class TeleportAura : public Module {
private:
    float reach;

public:
    TeleportAura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }
};