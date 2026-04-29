#pragma once
#include "../Module.h"

class WTap : public Module {
private:
    int ticksSinceAttack = 0;
    bool isWTapping = false;
    int wTapDelay = 2; // Ticks to wait before re-sprinting

public:
    WTap();
    void OnTick() override;
    
    int GetDelay() const { return wTapDelay; }
    void SetDelay(int d) { wTapDelay = d; }
};
