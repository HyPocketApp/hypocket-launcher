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

// Returns [checksum, elapsed milliseconds].
extern "C" JNIEXPORT jlongArray JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeRunWorkload(JNIEnv* env, jobject /* this */) {
    const hypocket::WorkloadResult result = hypocket::RunNativeWorkload();
    const jlong values[] = {static_cast<jlong>(result.checksum), static_cast<jlong>(result.elapsed_ms)};
    jlongArray array = env->NewLongArray(2);
    if (array != nullptr) env->SetLongArrayRegion(array, 0, 2, values);
    return array;
}
