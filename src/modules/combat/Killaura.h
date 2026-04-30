#pragma once
#include "../Module.h"
#include <jni.h>

class Killaura : public Module {
private:
    float reach;
    float fov;
    bool enable360;
    bool packetOrderOptimize;
    int ticksSinceLastAttack;
    int randomDelay;

    // Cached previous-tick player position for packet order optimization.
    // On attack frames we compute distance using last tick's position,
    // which is what the server sees before our movement packet arrives.
    double prevPx, prevPy, prevPz;
    bool hasPrevPos;

public:
    Killaura();
    void OnTick() override;

    float GetReach() const { return reach; }
    void SetReach(float r) { reach = r; }

    float GetFOV() const { return fov; }
    void SetFOV(float f) { fov = f; }

    bool Is360Enabled() const { return enable360; }
    void Set360Enabled(bool e) { enable360 = e; }

    bool IsPacketOrderOptimizeEnabled() const { return packetOrderOptimize; }
    void SetPacketOrderOptimizeEnabled(bool e) { packetOrderOptimize = e; }
};
