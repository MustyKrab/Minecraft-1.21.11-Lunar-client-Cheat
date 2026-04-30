#include "JNIHelper.h"
#include <windows.h>
#include <iostream>

JavaVM* JNIHelper::vm = nullptr;
thread_local JNIEnv* JNIHelper::env = nullptr;
jvmtiEnv* JNIHelper::jvmti = nullptr;

bool JNIHelper::Initialize() {
    HMODULE hJvm = GetModuleHandleA("jvm.dll");
    if (!hJvm) return false;

    typedef jint(JNICALL* GetCreatedJavaVMs_t)(JavaVM**, jsize, jsize*);
    GetCreatedJavaVMs_t GetCreatedJavaVMs = (GetCreatedJavaVMs_t)GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs");
    
    if (!GetCreatedJavaVMs) return false;

    jsize numVMs;
    GetCreatedJavaVMs(&vm, 1, &numVMs);
    if (numVMs == 0) return false;

    vm->AttachCurrentThread((void**)&env, nullptr);
    vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1_2);

    return env != nullptr && jvmti != nullptr;
}

void JNIHelper::Cleanup() {
    if (vm) {
        vm->DetachCurrentThread();
    }
}

jclass JNIHelper::FindClass(const char* name) {
    if (!env) return nullptr;
    jclass localCls = env->FindClass(name);
    if (localCls) {
        jclass globalCls = (jclass)env->NewGlobalRef(localCls);
        env->DeleteLocalRef(localCls);
        return globalCls;
    }
    return nullptr;
}

jclass JNIHelper::FindClassBySignature(const char* targetSig) {
    if (!jvmti || !env) return nullptr;

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmti->GetLoadedClasses(&classCount, &classes);

    jclass foundClass = nullptr;

    for (int i = 0; i < classCount; i++) {
        char* sig;
        jvmti->GetClassSignature(classes[i], &sig, nullptr);
        if (sig) {
            if (!foundClass && strcmp(sig, targetSig) == 0) {
                foundClass = (jclass)env->NewGlobalRef(classes[i]); // Make global ref
            }
            jvmti->Deallocate((unsigned char*)sig);
        }
        // FIX: DeleteLocalRef on classes[i] to prevent local ref table overflow
        // GetLoadedClasses returns an array of JNI local references.
        env->DeleteLocalRef(classes[i]); 
    }

    jvmti->Deallocate((unsigned char*)classes);
    return foundClass;
}

jclass JNIHelper::FindClassSafe(const char* sig, const char* fallbackName) {
    jclass cls = FindClassBySignature(sig);
    if (!cls && env) {
        jclass localCls = env->FindClass(fallbackName);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (localCls) {
            cls = (jclass)env->NewGlobalRef(localCls); // Make global ref
            env->DeleteLocalRef(localCls);
        }
    }
    return cls;
}

jfieldID JNIHelper::GetFieldSafe(jclass cls, const char* name, const char* sig, const char* fallbackName) {
    if (!cls || !env) return nullptr;
    jfieldID fid = env->GetFieldID(cls, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        fid = env->GetFieldID(cls, fallbackName, sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    return fid;
}

jfieldID JNIHelper::GetStaticFieldSafe(jclass cls, const char* name, const char* sig, const char* fallbackName) {
    if (!cls || !env) return nullptr;
    jfieldID fid = env->GetStaticFieldID(cls, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        fid = env->GetStaticFieldID(cls, fallbackName, sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    return fid;
}

jmethodID JNIHelper::GetMethodSafe(jclass cls, const char* name, const char* sig, const char* fallbackName) {
    if (!cls || !env) return nullptr;
    jmethodID mid = env->GetMethodID(cls, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mid = env->GetMethodID(cls, fallbackName, sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    return mid;
}
