#pragma once
#include "../Module.h"

class Killaura : public Module {
private:
    float reach;
    float aimbotIntensity;
    float fov;
    bool aimAssistMode;

public:
    Killaura();
    void OnTick() override;
    
    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }

    float GetAimbotIntensity() const { return aimbotIntensity; }
    void SetAimbotIntensity(float i) { aimbotIntensity = i; }
    
    float GetFOV() const { return fov; }
    void SetFOV(float f) { fov = f; }

    bool IsAimAssistMode() const { return aimAssistMode; }
    void SetAimAssistMode(bool m) { aimAssistMode = m; }
};