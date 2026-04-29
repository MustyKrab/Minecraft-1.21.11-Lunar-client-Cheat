#pragma once
#include <jni.h>
#include <jvmti.h>
#include <string>

class JNIHelper {
public:
    static JavaVM* vm;
    static JNIEnv* env;
    static jvmtiEnv* jvmti;

    static bool Initialize();
    static void Cleanup();
    
    static jclass FindClass(const char* name);
    static jclass FindClassBySignature(const char* sig);
};
