#include "Macro.h"
#include <windows.h>
#include <chrono>
#include <algorithm>

static std::mt19937 s_macroRng([]() -> uint32_t {
    std::random_device rd;
    return rd() ^ (uint32_t)(GetTickCount64() * 2654435761ULL);
}());

Macro::Macro() : Module("Macro") {}

long long Macro::GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

int Macro::GaussianSleep(double mean, double stddev, int min_val, int max_val) {
    if (stddev <= 0.001) stddev = 0.001;
    std::normal_distribution<> distr(mean, stddev);
    int val = (int)std::round(distr(s_macroRng));
    return std::clamp(val, min_val, max_val);
}

void Macro::SendKeyDownEx(WORD vKey) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &input, sizeof(INPUT));
}

void Macro::SendKeyUpEx(WORD vKey) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void Macro::SendMouseDownEx() {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
}

void Macro::SendMouseUpEx() {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

void Macro::OnTick() {
    long long currentTime = GetTimeMs();

    // --- StunSlam State Machine ---
    if (stunSlamEnabled) {
        if (stunSlamState == 0 && (GetAsyncKeyState(VK_XBUTTON2) & 0x8000)) {
            // Start StunSlam
            SendKeyDownEx('1');
            stunSlamNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            stunSlamState = 1;
        }
        else if (stunSlamState == 1 && currentTime >= stunSlamNextTime) {
            SendMouseDownEx();
            stunSlamNextTime = currentTime + GaussianSleep(15.0, 1.5, 12, 18);
            stunSlamState = 2;
        }
        else if (stunSlamState == 2 && currentTime >= stunSlamNextTime) {
            SendKeyDownEx('9');
            stunSlamNextTime = currentTime + GaussianSleep(10.0, 1.0, 8, 13);
            stunSlamState = 3;
        }
        else if (stunSlamState == 3 && currentTime >= stunSlamNextTime) {
            SendMouseUpEx();
            stunSlamNextTime = currentTime + GaussianSleep(15.0, 1.5, 12, 18);
            stunSlamState = 4;
        }
        else if (stunSlamState == 4 && currentTime >= stunSlamNextTime) {
            SendMouseDownEx();
            stunSlamNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            stunSlamState = 5;
        }
        else if (stunSlamState == 5 && currentTime >= stunSlamNextTime) {
            SendMouseUpEx();
            SendKeyUpEx('1');
            SendKeyUpEx('9');
            // Cooldown before next sequence
            stunSlamNextTime = currentTime + GaussianSleep(350.0, 25.0, 300, 450);
            stunSlamState = 6;
        }
        else if (stunSlamState == 6 && currentTime >= stunSlamNextTime) {
            // Ready for next trigger
            stunSlamState = 0;
        }
    } else {
        stunSlamState = 0; // Reset if disabled mid-macro
    }

    // --- SpearDash State Machine ---
    if (spearDashEnabled) {
        if (spearDashState == 0 && (GetAsyncKeyState('2') & 0x8000)) {
            // Wait for server to register spear equip
            spearDashNextTime = currentTime + GaussianSleep(50.0, 3.0, 45, 55);
            spearDashState = 1;
        }
        else if (spearDashState == 1 && currentTime >= spearDashNextTime) {
            SendKeyDownEx('4');
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 2;
        }
        else if (spearDashState == 2 && currentTime >= spearDashNextTime) {
            SendMouseDownEx();
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 3;
        }
        else if (spearDashState == 3 && currentTime >= spearDashNextTime) {
            SendKeyDownEx('2');
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 4;
        }
        else if (spearDashState == 4 && currentTime >= spearDashNextTime) {
            SendMouseUpEx();
            SendKeyUpEx('4');
            SendKeyUpEx('2');
            // Cooldown before next sequence
            spearDashNextTime = currentTime + GaussianSleep(250.0, 20.0, 200, 300);
            spearDashState = 5;
        }
        else if (spearDashState == 5 && currentTime >= spearDashNextTime) {
            // Ready for next trigger
            spearDashState = 0;
        }
    } else {
        spearDashState = 0; // Reset if disabled mid-macro
    }
}
