#pragma once
#include <jni.h>
#include <jvmti.h>
#include <string>

class JNIHelper {
public:
    static JavaVM* vm;
    static thread_local JNIEnv* env;
    static jvmtiEnv* jvmti;

    static bool Initialize();
    static void Cleanup();
    
    static jclass FindClass(const char* name);
    static jclass FindClassBySignature(const char* sig);
    
    // Safe resolution methods that fallback to mapped names and clear exceptions
    static jclass FindClassSafe(const char* sig, const char* fallbackName);
    static jfieldID GetFieldSafe(jclass cls, const char* name, const char* sig, const char* fallbackName);
    static jfieldID GetStaticFieldSafe(jclass cls, const char* name, const char* sig, const char* fallbackName);
    static jmethodID GetMethodSafe(jclass cls, const char* name, const char* sig, const char* fallbackName);
};
