#pragma once
#include "../Module.h"
#include <random>
#include <windows.h>
#include <vector>

class Macro : public Module {
private:
    bool stunSlamEnabled = false;
    bool spearDashEnabled = false;

    int stunSlamState = 0;
    long long stunSlamNextTime = 0;
    char currentAxeKey = '1';
    char currentMaceKey = '9';
    
    int spearDashState = 0;
    long long spearDashNextTime = 0;
    char currentNoCdKey = '1';
    char currentSpearKey = '2';

    int GaussianSleep(double mean, double stddev, int min_val, int max_val);
    void SendKeyDownEx(WORD vKey);
    void SendKeyUpEx(WORD vKey);
    void SendMouseDownEx();
    void SendMouseUpEx();
    long long GetTimeMs();
    
    std::vector<int> GetItemSlots(void* env_ptr, void* inventory_ptr, const char* itemKey);

public:
    Macro();
    void OnTick() override;

    bool IsStunSlamEnabled() const { return stunSlamEnabled; }
    void SetStunSlamEnabled(bool e) { stunSlamEnabled = e; }
    
    bool IsSpearDashEnabled() const { return spearDashEnabled; }
    void SetSpearDashEnabled(bool e) { spearDashEnabled = e; }
};
