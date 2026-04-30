#pragma once
#include "Module.h"

class FakeLag : public Module {
private:
    int chokeLimit;
    int chokedPackets;

public:
    FakeLag();
    void OnTick() override;
    
    int GetChokeLimit() const { return chokeLimit; }
    void SetChokeLimit(int limit) { chokeLimit = limit; }
};