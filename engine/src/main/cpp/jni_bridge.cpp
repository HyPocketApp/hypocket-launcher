// Thin JNI layer between Kotlin and the native engine. Keep logic out of this
// file: it only converts types and forwards calls.
#include <jni.h>

#include "hypocket_engine.h"

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeVersion(JNIEnv* env, jobject /* this */) {
    return env->NewStringUTF(hypocket::EngineVersion());
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeAbi(JNIEnv* env, jobject /* this */) {
    return env->NewStringUTF(hypocket::EngineAbi());
}
