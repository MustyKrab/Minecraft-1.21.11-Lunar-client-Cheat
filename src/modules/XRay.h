#pragma once
#include <jni.h>

namespace Modules {
    class XRay {
    public:
        static bool bEnabled;
        static bool bNeedsUpdate;

        static void Toggle();
        static void ProcessSafeUpdate(JNIEnv* env);
    };
}
