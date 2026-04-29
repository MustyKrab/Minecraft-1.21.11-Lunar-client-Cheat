#pragma once
#include "../Module.h"

class XRay : public Module {
public:
    XRay();
    void OnEnable() override;
    void OnDisable() override;
    void OnTick() override;
};
