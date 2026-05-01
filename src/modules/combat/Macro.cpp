#include "Macro.h"
#include "../../core/JNIHelper.h"
#include <windows.h>
#include <chrono>
#include <algorithm>
#include <string>

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

// JNI Helper to scan hotbar slots (0-8) for a specific item, optionally checking for an enchantment
int Macro::FindHotbarSlot(void* env_ptr, void* inventory_ptr, const char* itemKey, const char* enchKey) {
    JNIEnv* env = (JNIEnv*)env_ptr;
    jobject inventory = (jobject)inventory_ptr;
    if (!env || !inventory) return -1;

    jclass invClass = env->GetObjectClass(inventory);
    jmethodID getStack = env->GetMethodID(invClass, "method_5438", "(I)Lnet/minecraft/class_1799;");
    env->ExceptionClear();
    if (!getStack) { env->DeleteLocalRef(invClass); return -1; }

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
                        
                        bool match = (lowerStr.find(lowerItem) != std::string::npos);
                        env->ReleaseStringUTFChars(jKey, keyStr);
                        env->DeleteLocalRef(jKey);

                        if (match && enchKey != nullptr) {
                            match = false; // Assume false until we find the enchantment
                            // Get ComponentMap: method_57353 ()Lnet/minecraft/class_9323;
                            jmethodID getComponents = env->GetMethodID(stackClass, "method_57353", "()Lnet/minecraft/class_9323;");
                            env->ExceptionClear();
                            if (getComponents) {
                                jobject compMap = env->CallObjectMethod(stack, getComponents);
                                env->ExceptionClear();
                                if (compMap) {
                                    jclass compMapClass = env->GetObjectClass(compMap);
                                    jmethodID mapToString = env->GetMethodID(compMapClass, "toString", "()Ljava/lang/String;");
                                    env->ExceptionClear();
                                    if (mapToString) {
                                        jstring jCompStr = (jstring)env->CallObjectMethod(compMap, mapToString);
                                        env->ExceptionClear();
                                        if (jCompStr) {
                                            const char* compStr = env->GetStringUTFChars(jCompStr, 0);
                                            std::string lowerComp = compStr;
                                            std::transform(lowerComp.begin(), lowerComp.end(), lowerComp.begin(), ::tolower);
                                            
                                            std::string lowerEnch = enchKey;
                                            std::transform(lowerEnch.begin(), lowerEnch.end(), lowerEnch.begin(), ::tolower);
                                            
                                            if (lowerComp.find(lowerEnch) != std::string::npos) {
                                                match = true;
                                            }
                                            env->ReleaseStringUTFChars(jCompStr, compStr);
                                            env->DeleteLocalRef(jCompStr);
                                        }
                                    }
                                    env->DeleteLocalRef(compMapClass);
                                    env->DeleteLocalRef(compMap);
                                }
                            }
                        }

                        if (match) {
                            env->DeleteLocalRef(itemClass);
                            env->DeleteLocalRef(item);
                            env->DeleteLocalRef(stackClass);
                            env->DeleteLocalRef(stack);
                            env->DeleteLocalRef(invClass);
                            return i; // Found slot 0-8
                        }
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
    return -1;
}

void Macro::OnTick() {
    long long currentTime = GetTimeMs();
    JNIEnv* env = nullptr;
    
    // Only attach JNI if we are actively triggering a macro to save CPU
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
                        // field_1724 = player
                        jfieldID playerField = env->GetFieldID(mcClass, "field_1724", "Lnet/minecraft/class_746;");
                        env->ExceptionClear();
                        if (playerField) {
                            jobject player = env->GetObjectField(mc, playerField);
                            env->ExceptionClear();
                            if (player) {
                                jclass playerClass = env->GetObjectClass(player);
                                // field_7514 = inventory
                                jfieldID invField = env->GetFieldID(playerClass, "field_7514", "Lnet/minecraft/class_1661;");
                                env->ExceptionClear();
                                if (invField) {
                                    jobject inventory = env->GetObjectField(player, invField);
                                    env->ExceptionClear();
                                    
                                    if (inventory) {
                                        // --- Dynamic Hotbar Resolution ---
                                        
                                        // Find Axe (any type)
                                        int axeSlot = FindHotbarSlot(env, inventory, "axe", nullptr);
                                        if (axeSlot != -1) currentAxeKey = '1' + axeSlot;
                                        
                                        // Find Spear
                                        int spearSlot = FindHotbarSlot(env, inventory, "spear", nullptr);
                                        if (spearSlot != -1) currentSpearKey = '1' + spearSlot;
                                        
                                        // Find No-CD item (fallback to Axe if none found)
                                        currentNoCdKey = currentAxeKey; 

                                        // Fall distance check for Mace logic
                                        // field_6017 = fallDistance in class_1297 (Entity)
                                        jclass entityClass = env->FindClass("net/minecraft/class_1297");
                                        env->ExceptionClear();
                                        double fallDistance = 0.0;
                                        if (entityClass) {
                                            // Try double first (1.21.11)
                                            jfieldID fallDistField = env->GetFieldID(entityClass, "field_6017", "D");
                                            env->ExceptionClear();
                                            if (fallDistField) {
                                                fallDistance = env->GetDoubleField(player, fallDistField);
                                                env->ExceptionClear();
                                            } else {
                                                // Fallback to float
                                                fallDistField = env->GetFieldID(entityClass, "field_6017", "F");
                                                env->ExceptionClear();
                                                if (fallDistField) {
                                                    fallDistance = (double)env->GetFloatField(player, fallDistField);
                                                    env->ExceptionClear();
                                                }
                                            }
                                            env->DeleteLocalRef(entityClass);
                                        }

                                        int maceSlot = -1;
                                        if (fallDistance <= 9.0) {
                                            // <= 9 blocks: Try to find Breach mace first
                                            maceSlot = FindHotbarSlot(env, inventory, "mace", "breach");
                                        } else {
                                            // > 9 blocks: Try to find Density mace first
                                            maceSlot = FindHotbarSlot(env, inventory, "mace", "density");
                                        }
                                        
                                        // Fallback if specific enchanted mace wasn't found
                                        if (maceSlot == -1) {
                                            maceSlot = FindHotbarSlot(env, inventory, "mace", nullptr);
                                        }
                                        
                                        if (maceSlot != -1) currentMaceKey = '1' + maceSlot;

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
