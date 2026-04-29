#pragma once
#include "../Module.h"
#include <vector>

struct XRayBlock {
    int x, y, z;
    int r, g, b;
};

class XRay : public Module {
private:
    std::vector<XRayBlock> foundBlocks;
    int scanRadius = 32;
    int tickCounter = 0;

public:
    bool showDiamond = true;
    bool showGold = true;
    bool showIron = true;
    bool showEmerald = true;
    bool showNetherite = true;
    bool showChests = true;
    bool showEnderChests = true;
    bool showSpawners = true;
    bool showHoppers = true;

    XRay();
    void OnTick() override;
    
    const std::vector<XRayBlock>& GetFoundBlocks() const { return foundBlocks; }
};
