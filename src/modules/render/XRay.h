#pragma once
#include "../Module.h"
#include <vector>
#include <mutex>

struct XRayBlock {
    int x, y, z;
    int r, g, b;
};

class XRay : public Module {
private:
    std::vector<XRayBlock> foundBlocks;
    mutable std::mutex blocksMutex;
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
    
    std::vector<XRayBlock> GetFoundBlocks() const {
        std::lock_guard<std::mutex> lock(blocksMutex);
        return foundBlocks;
    }
    
    void SetFoundBlocks(const std::vector<XRayBlock>& blocks) {
        std::lock_guard<std::mutex> lock(blocksMutex);
        foundBlocks = blocks;
    }
};
