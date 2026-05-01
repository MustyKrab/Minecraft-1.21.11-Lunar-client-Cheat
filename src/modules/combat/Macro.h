#pragma once
#include "../Module.h"
#include <random>
#include <windows.h> // FIX: Added to define WORD

class Macro : public Module {
private:
    bool stunSlamEnabled = false;
    bool spearDashEnabled = false;
    
    // State machine for StunSlam
    int stunSlamState = 0;
    long long stunSlamNextTime = 0;
    
    // State machine for SpearDash
    int spearDashState = 0;
    long long spearDashNextTime = 0;

    int GaussianSleep(double mean, double stddev, int min_val, int max_val);
    void SendKeyDownEx(WORD vKey);
    void SendKeyUpEx(WORD vKey);
    void SendMouseDownEx();
    void SendMouseUpEx();
    long long GetTimeMs();

public:
    Macro();
    void OnTick() override;
    
    bool IsStunSlamEnabled() const { return stunSlamEnabled; }
    void SetStunSlamEnabled(bool e) { stunSlamEnabled = e; }
    
    bool IsSpearDashEnabled() const { return spearDashEnabled; }
    void SetSpearDashEnabled(bool e) { spearDashEnabled = e; }
};
