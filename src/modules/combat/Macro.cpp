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

int Macro::FindHotbarSlot(void* env_ptr, void* inventory_ptr, const char* itemKey, const char* enchKey) {
    JNIEnv* env = (JNIEnv*)env_ptr;
    jobject inventory = (jobject)inventory_ptr;
    if (!env || !inventory) return -1;

    jclass invClass = env->GetObjectClass(inventory);
    jmethodID getStack = env->GetMethodID(invClass, "method_5438", "(I)Lnet/minecraft/class_1799;");
    env->ExceptionClear();
    if (!getStack) { env->DeleteLocalRef(invClass); return -1; }

    int fallbackSlot = -1;

    for (int i = 0; i < 9; i++) {
        jobject stack = env->CallObjectMethod(inventory, getStack, i);
        env->ExceptionClear();
        if (!stack) continue;

        jclass stackClass = env->GetObjectClass(stack);
        // Fixed the signature for getItem in 1.21.
        jmethodID getItem = env->GetMethodID(stackClass, "method_7909", "()Lnet/minecraft/class_1792;");
        env->ExceptionClear();
        
        if (getItem) {
            jobject item = env->CallObjectMethod(stack, getItem);
            env->ExceptionClear();
            if (item) {
                jclass itemClass = env->GetObjectClass(item);
                // Fixed the signature for getTranslationKey in 1.21 to take ItemStack as arg
                jmethodID getTranslationKey = env->GetMethodID(itemClass, "method_7866", "(Lnet/minecraft/class_1799;)Ljava/lang/String;");
                env->ExceptionClear();
                
                if (getTranslationKey) {
                    // Pass the stack as an argument
                    jstring jKey = (jstring)env->CallObjectMethod(item, getTranslationKey, stack);
                    env->ExceptionClear();
                    if (jKey) {
                        const char* keyStr = env->GetStringUTFChars(jKey, 0);
                        std::string lowerStr = keyStr;
                        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
                        
                        std::string lowerItem = itemKey;
                        std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
                        
                        bool isCorrectItem = (lowerStr.find(lowerItem) != std::string::npos);
                        env->ReleaseStringUTFChars(jKey, keyStr);
                        env->DeleteLocalRef(jKey);

                        if (isCorrectItem) {
                            if (enchKey == nullptr) {
                                env->DeleteLocalRef(itemClass);
                                env->DeleteLocalRef(item);
                                env->DeleteLocalRef(stackClass);
                                env->DeleteLocalRef(stack);
                                env->DeleteLocalRef(invClass);
                                return i;
                            }

                            bool hasEnch = false;
                            std::string lowerEnch = enchKey;
                            std::transform(lowerEnch.begin(), lowerEnch.end(), lowerEnch.begin(), ::tolower);

                            jmethodID getComponents = env->GetMethodID(stackClass, "method_57331", "()Lnet/minecraft/class_9323;");
                            env->ExceptionClear();
                            
                            if (getComponents) {
                                jobject componentMap = env->CallObjectMethod(stack, getComponents);
                                env->ExceptionClear();
                                
                                if (componentMap) {
                                    jclass dataComponentTypesClass = env->FindClass("net/minecraft/class_9331");
                                    env->ExceptionClear();
                                    
                                    if (dataComponentTypesClass) {
                                        // field_49574 = ENCHANTMENTS
                                        jfieldID enchantmentsField = env->GetStaticFieldID(dataComponentTypesClass, "field_49574", "LNet/minecraft/class_9331;");
                                        env->ExceptionClear();
                                        
                                        if (enchantmentsField) {
                                            jobject enchantmentsType = env->GetStaticObjectField(dataComponentTypesClass, enchantmentsField);
                                            env->ExceptionClear();
                                            
                                            if (enchantmentsType) {
                                                jclass componentMapClass = env->GetObjectClass(componentMap);
                                                jmethodID get = env->GetMethodID(componentMapClass, "method_57334", "(Lnet/minecraft/class_9331;)Ljava/lang/Object;");
                                                env->ExceptionClear();
                                                
                                                if (get) {
                                                    jobject itemEnchantments = env->CallObjectMethod(componentMap, get, enchantmentsType);
                                                    env->ExceptionClear();
                                                    
                                                    if (itemEnchantments) {
                                                        jclass itemEnchClass = env->GetObjectClass(itemEnchantments);
                                                        jmethodID toString = env->GetMethodID(itemEnchClass, "toString", "()Ljava/lang/String;");
                                                        env->ExceptionClear();
                                                        
                                                        if (toString) {
                                                            jstring jEnchStr = (jstring)env->CallObjectMethod(itemEnchantments, toString);
                                                            env->ExceptionClear();
                                                            
                                                            if (jEnchStr) {
                                                                const char* enchStr = env->GetStringUTFChars(jEnchStr, 0);
                                                                std::string fullEnchStr = enchStr;
                                                                std::transform(fullEnchStr.begin(), fullEnchStr.end(), fullEnchStr.begin(), ::tolower);
                                                                
                                                                if (fullEnchStr.find(lowerEnch) != std::string::npos) {
                                                                    hasEnch = true;
                                                                }
                                                                env->ReleaseStringUTFChars(jEnchStr, enchStr);
                                                                env->DeleteLocalRef(jEnchStr);
                                                            }
                                                        }
                                                        env->DeleteLocalRef(itemEnchClass);
                                                        env->DeleteLocalRef(itemEnchantments);
                                                    }
                                                }
                                                env->DeleteLocalRef(componentMapClass);
                                            }
                                            env->DeleteLocalRef(enchantmentsType);
                                        }
                                        env->DeleteLocalRef(dataComponentTypesClass);
                                    }
                                    env->DeleteLocalRef(componentMap);
                                }
                            }
                            
                            if (hasEnch) {
                                env->DeleteLocalRef(itemClass);
                                env->DeleteLocalRef(item);
                                env->DeleteLocalRef(stackClass);
                                env->DeleteLocalRef(stack);
                                env->DeleteLocalRef(invClass);
                                return i;
                            }
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
    return -1; // No fallback, return -1 if not found
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
                                        double fallDistance = 0.0;
                                        jfieldID fallDistFieldD = env->GetFieldID(playerClass, "field_6017", "D");
                                        env->ExceptionClear();
                                        if (fallDistFieldD) {
                                            fallDistance = env->GetDoubleField(player, fallDistFieldD);
                                            env->ExceptionClear();
                                        } else {
                                            jfieldID fallDistFieldF = env->GetFieldID(playerClass, "field_6017", "F");
                                            env->ExceptionClear();
                                            if (fallDistFieldF) {
                                                fallDistance = (double)env->GetFloatField(player, fallDistFieldF);
                                                env->ExceptionClear();
                                            }
                                        }

                                        int maceSlot = -1;
                                        if (fallDistance <= 9.0) {
                                            // <= 9 blocks: Try to find Breach mace first
                                            maceSlot = FindHotbarSlot(env, inventory, "mace", "breach");
                                        } else {
                                            // > 9 blocks: Try to find Density mace first
                                            maceSlot = FindHotbarSlot(env, inventory, "mace", "density");
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
