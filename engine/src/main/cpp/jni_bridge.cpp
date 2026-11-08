// Thin JNI layer between Kotlin and the native engine. Keep logic out of this
// file: it only converts types and forwards calls.
#include <jni.h>

#include <android/native_window_jni.h>

#include "audio_output.h"
#include "direct_display.h"
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

// Direct display (see direct_display.cpp). The handle is a pointer kept by Kotlin.
extern "C" JNIEXPORT jlong JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeDisplayCreate(JNIEnv* env, jobject /* this */, jobject surface,
                                                                   jobjectArray socket_name_out) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) return 0;
    char name[64];
    auto* handle = hypocket::DirectDisplayCreate(window, name, sizeof(name));
    if (handle == nullptr) {
        ANativeWindow_release(window);
        return 0;
    }
    env->SetObjectArrayElement(socket_name_out, 0, env->NewStringUTF(name));
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeDisplayResize(JNIEnv* /* env */, jobject /* this */, jlong handle,
                                                                   jint width, jint height) {
    hypocket::DirectDisplayResize(reinterpret_cast<std::shared_ptr<hypocket::State>*>(handle), width, height);
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeDisplayShownFrames(JNIEnv* /* env */, jobject /* this */,
                                                                        jlong handle) {
    return hypocket::DirectDisplayShownFrames(reinterpret_cast<std::shared_ptr<hypocket::State>*>(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeDisplayDestroy(JNIEnv* /* env */, jobject /* this */,
                                                                    jlong handle) {
    hypocket::DirectDisplayDestroy(reinterpret_cast<std::shared_ptr<hypocket::State>*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeDisplayInfo(JNIEnv* env, jobject /* this */, jlong handle) {
    return env->NewStringUTF(
        hypocket::DirectDisplayInfo(reinterpret_cast<std::shared_ptr<hypocket::State>*>(handle)).c_str());
}

// Audio bridge (see audio_output.cpp). The handle is a pointer kept by Kotlin.
extern "C" JNIEXPORT jlong JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeAudioCreate(JNIEnv* env, jobject /* this */, jstring path) {
    const char* chars = env->GetStringUTFChars(path, nullptr);
    if (chars == nullptr) return 0;
    auto* handle = hypocket::AudioOutputCreate(chars);
    env->ReleaseStringUTFChars(path, chars);
    return reinterpret_cast<jlong>(handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeAudioInfo(JNIEnv* env, jobject /* this */, jlong handle) {
    auto* state = reinterpret_cast<std::shared_ptr<hypocket::AudioState>*>(handle);
    return env->NewStringUTF(hypocket::AudioOutputInfo(state).c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_hypocketapp_engine_NativeBridge_nativeAudioDestroy(JNIEnv* /* env */, jobject /* this */, jlong handle) {
    hypocket::AudioOutputDestroy(reinterpret_cast<std::shared_ptr<hypocket::AudioState>*>(handle));
}
