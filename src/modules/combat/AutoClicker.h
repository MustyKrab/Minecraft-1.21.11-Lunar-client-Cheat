#pragma once
#include "../Module.h"

class AutoClicker : public Module {
private:
    float minCps = 10.0f;
    float maxCps = 14.0f;
    bool jitter = true;
    
    long long nextClickTime = 0;
    bool isClicking = false;

    long long GetTimeMs();
    int GetRandomDelay();

public:
    AutoClicker();
    void OnTick() override;
    
    float GetMinCps() const { return minCps; }
    void SetMinCps(float c) { minCps = c; }
    
    float GetMaxCps() const { return maxCps; }
    void SetMaxCps(float c) { maxCps = c; }
    
    bool IsJitterEnabled() const { return jitter; }
    void SetJitter(bool j) { jitter = j; }
};
