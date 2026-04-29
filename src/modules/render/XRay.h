#pragma once
#include "../Module.h"
#include <vector>

struct BlockPos {
    int x, y, z;
};

class XRay : public Module {
private:
    std::vector<BlockPos> foundBlocks;
    int scanRadius = 32;
    int tickCounter = 0;

public:
    XRay();
    void OnTick() override;
    
    const std::vector<BlockPos>& GetFoundBlocks() const { return foundBlocks; }
};
