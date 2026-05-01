#include "Macro.h"
#include "../../core/JNIHelper.h"
#include <windows.h>
#include <chrono>
#include <algorithm>
#include <string>
#include <vector>

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

// Returns a list of hotbar slots containing the specified item
std::vector<int> Macro::GetItemSlots(void* env_ptr, void* inventory_ptr, const char* itemKey) {
    JNIEnv* env = (JNIEnv*)env_ptr;
    jobject inventory = (jobject)inventory_ptr;
    std::vector<int> slots;
    if (!env || !inventory) return slots;

    jclass invClass = env->GetObjectClass(inventory);
    jmethodID getStack = env->GetMethodID(invClass, "method_5438", "(I)Lnet/minecraft/class_1799;");
    env->ExceptionClear();
    if (!getStack) { env->DeleteLocalRef(invClass); return slots; }

    for (int i = 0; i < 9; i++) {
        jobject stack = env->CallObjectMethod(inventory, getStack, i);
        env->ExceptionClear();
        if (!stack) continue;

        jclass stackClass = env->GetObjectClass(stack);
        jmethodID getItem = env->GetMethodID(stackClass, "method_7909", "()Lnet/minecraft/class_1792;");
        env->ExceptionClear();
        
        if (getItem) {
            jobject item = env->CallObjectMethod(stack, getItem);
            env->ExceptionClear();
            if (item) {
                jclass itemClass = env->GetObjectClass(item);
                jmethodID getTranslationKey = env->GetMethodID(itemClass, "method_7866", "()Ljava/lang/String;");
                env->ExceptionClear();
                
                if (getTranslationKey) {
                    jstring jKey = (jstring)env->CallObjectMethod(item, getTranslationKey);
                    env->ExceptionClear();
                    if (jKey) {
                        const char* keyStr = env->GetStringUTFChars(jKey, 0);
                        std::string lowerStr = keyStr;
                        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
                        
                        std::string lowerItem = itemKey;
                        std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
                        
                        if (lowerStr.find(lowerItem) != std::string::npos) {
                            slots.push_back(i);
                        }
                        env->ReleaseStringUTFChars(jKey, keyStr);
                        env->DeleteLocalRef(jKey);
                    }
                }
                env->DeleteLocalRef(itemClass);
                env->DeleteLocalRef(item);
            }
        }
        env->DeleteLocalRef(stackClass);
        env->DeleteLocalRef(stack);
    }
    env->DeleteLocalRef(invClass);
    return slots;
}

void Macro::OnTick() {
    long long currentTime = GetTimeMs();
    JNIEnv* env = nullptr;
    
    if ((stunSlamEnabled && stunSlamState == 0 && (GetAsyncKeyState(VK_XBUTTON2) & 0x8000)) || 
        (spearDashEnabled && spearDashState == 0 && (GetAsyncKeyState('2') & 0x8000))) {
        
        if (JNIHelper::vm) {
            JNIHelper::vm->AttachCurrentThread((void**)&env, nullptr);
        }
        
        if (env) {
            jclass mcClass = env->FindClass("net/minecraft/class_310");
            env->ExceptionClear();
            if (mcClass) {
                jmethodID getInstance = env->GetStaticMethodID(mcClass, "method_1551", "()Lnet/minecraft/class_310;");
                env->ExceptionClear();
                if (getInstance) {
                    jobject mc = env->CallStaticObjectMethod(mcClass, getInstance);
                    env->ExceptionClear();
                    if (mc) {
                        jfieldID playerField = env->GetFieldID(mcClass, "field_1724", "Lnet/minecraft/class_746;");
                        env->ExceptionClear();
                        if (playerField) {
                            jobject player = env->GetObjectField(mc, playerField);
                            env->ExceptionClear();
                            if (player) {
                                jclass playerClass = env->GetObjectClass(player);
                                jfieldID invField = env->GetFieldID(playerClass, "field_7514", "Lnet/minecraft/class_1661;");
                                env->ExceptionClear();
                                if (invField) {
                                    jobject inventory = env->GetObjectField(player, invField);
                                    env->ExceptionClear();
                                    
                                    if (inventory) {
                                        // Find Axe
                                        std::vector<int> axes = GetItemSlots(env, inventory, "axe");
                                        if (!axes.empty()) currentAxeKey = '1' + axes[0];
                                        
                                        // Find Spear
                                        std::vector<int> spears = GetItemSlots(env, inventory, "spear");
                                        if (!spears.empty()) currentSpearKey = '1' + spears[0];
                                        
                                        currentNoCdKey = currentAxeKey; 

                                        // Fall distance check
                                        jclass entityClass = env->FindClass("net/minecraft/class_1297");
                                        env->ExceptionClear();
                                        double fallDistance = 0.0;
                                        if (entityClass) {
                                            jfieldID fallDistField = env->GetFieldID(entityClass, "field_6017", "D");
                                            env->ExceptionClear();
                                            if (fallDistField) {
                                                fallDistance = env->GetDoubleField(player, fallDistField);
                                                env->ExceptionClear();
                                            } else {
                                                fallDistField = env->GetFieldID(entityClass, "field_6017", "F");
                                                env->ExceptionClear();
                                                if (fallDistField) {
                                                    fallDistance = (float)env->GetDoubleField(player, fallDistField);
                                                    env->ExceptionClear();
                                                }
                                            }
                                            env->DeleteLocalRef(entityClass);
                                        }

                                        // Find all Maces
                                        std::vector<int> maces = GetItemSlots(env, inventory, "mace");
                                        
                                        if (!maces.empty()) {
                                            if (maces.size() == 1) {
                                                // Only one mace, use it regardless of fall distance
                                                currentMaceKey = '1' + maces[0];
                                            } else {
                                                // Multiple maces found. Apply heuristic:
                                                // Usually players organize hotbar left-to-right.
                                                // Let's assume Breach is the primary (leftmost) and Density is secondary (rightmost).
                                                // Or vice versa based on common PvP layouts.
                                                // We will assume: Leftmost = Breach, Rightmost = Density
                                                if (fallDistance <= 9.0) {
                                                    // Want Breach (Leftmost)
                                                    currentMaceKey = '1' + maces.front();
                                                } else {
                                                    // Want Density (Rightmost)
                                                    currentMaceKey = '1' + maces.back();
                                                }
                                            }
                                        }

                                        env->DeleteLocalRef(inventory);
                                    }
                                }
                                env->DeleteLocalRef(playerClass);
                                env->DeleteLocalRef(player);
                            }
                        }
                        env->DeleteLocalRef(mc);
                    }
                }
                env->DeleteLocalRef(mcClass);
            }
        }
    }

    // --- StunSlam State Machine ---
    if (stunSlamEnabled) {
        if (stunSlamState == 0 && (GetAsyncKeyState(VK_XBUTTON2) & 0x8000)) {
            SendKeyDownEx(currentAxeKey);
            stunSlamNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            stunSlamState = 1;
        }
        else if (stunSlamState == 1 && currentTime >= stunSlamNextTime) {
            SendMouseDownEx();
            stunSlamNextTime = currentTime + GaussianSleep(15.0, 1.5, 12, 18);
            stunSlamState = 2;
        }
        else if (stunSlamState == 2 && currentTime >= stunSlamNextTime) {
            SendMouseUpEx();
            stunSlamNextTime = currentTime + GaussianSleep(10.0, 1.0, 8, 13);
            stunSlamState = 3;
        }
        else if (stunSlamState == 3 && currentTime >= stunSlamNextTime) {
            SendKeyDownEx(currentMaceKey);
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
            SendKeyUpEx(currentAxeKey);
            SendKeyUpEx(currentMaceKey);
            stunSlamNextTime = currentTime + GaussianSleep(350.0, 25.0, 300, 450);
            stunSlamState = 6;
        }
        else if (stunSlamState == 6 && currentTime >= stunSlamNextTime) {
            stunSlamState = 0;
        }
    } else {
        stunSlamState = 0;
    }

    // --- SpearDash Attribute Swapping State Machine ---
    if (spearDashEnabled) {
        if (spearDashState == 0 && (GetAsyncKeyState('2') & 0x8000)) {
            SendKeyDownEx(currentNoCdKey);
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 1;
        }
        else if (spearDashState == 1 && currentTime >= spearDashNextTime) {
            SendMouseDownEx();
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 2;
        }
        else if (spearDashState == 2 && currentTime >= spearDashNextTime) {
            SendKeyDownEx(currentSpearKey);
            spearDashNextTime = currentTime + GaussianSleep(12.0, 1.0, 10, 15);
            spearDashState = 3;
        }
        else if (spearDashState == 3 && currentTime >= spearDashNextTime) {
            SendMouseUpEx();
            SendKeyUpEx(currentNoCdKey);
            SendKeyUpEx(currentSpearKey);
            spearDashNextTime = currentTime + GaussianSleep(250.0, 20.0, 200, 300);
            spearDashState = 4;
        }
        else if (spearDashState == 4 && currentTime >= spearDashNextTime) {
            spearDashState = 0;
        }
    } else {
        spearDashState = 0;
    }
}
