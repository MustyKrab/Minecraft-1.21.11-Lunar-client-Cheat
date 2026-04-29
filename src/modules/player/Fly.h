#pragma once
#include "../Module.h"

class Fly : public Module {
public:
    Fly();
    void OnTick() override;
    void OnDisable() override;
};
