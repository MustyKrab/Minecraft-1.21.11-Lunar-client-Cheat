#pragma once
#include "../Module.h"

class Aimbot : public Module {
private:
    float fov;
    float smoothSpeed;

public:
    Aimbot();
    void OnTick() override;
    
    float GetFOV() const { return fov; }
    void SetFOV(float f) { fov = f; }
    
    float GetSmoothSpeed() const { return smoothSpeed; }
    void SetSmoothSpeed(float s) { smoothSpeed = s; }
};
