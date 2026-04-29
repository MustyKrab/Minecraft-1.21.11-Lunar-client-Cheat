#pragma once
#include "../Module.h"

class ESP : public Module {
public:
    ESP();
    void OnTick() override;
};
