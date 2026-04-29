#pragma once
#include "../Module.h"

class Killaura : public Module {
private:
    float reach;
    float aimbotIntensity;
    bool aimAssistMode;

public:
    Killaura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }

    float GetAimbotIntensity() const { return aimbotIntensity; }
    void SetAimbotIntensity(float i) { aimbotIntensity = i; }

    bool IsAimAssistMode() const { return aimAssistMode; }
    void SetAimAssistMode(bool m) { aimAssistMode = m; }
};