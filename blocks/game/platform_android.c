/* HyPocket Blocks, native on Android: the game built into libblocks.so and
 * run by the app (NativeGame.kt) with a window from a SurfaceView, an
 * OpenGL ES 3.0 context and AAudio for the sound. No emulator in the way.
 *
 * The app talks to the game as in the emulated build: the shared input file
 * and the environment (HYPOCKET_INPUT_PATH and the others), set here before
 * blocks_main() runs on the calling thread. The game's output goes to a log
 * file the app reads. The game keeps its state in globals, so it runs once
 * per process: the app gives it a process of its own and ends it after. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "platform.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static ANativeWindow* next_window; /* set by the app, taken by the game's thread */
static int window_changed, quit_requested;

static EGLDisplay display = EGL_NO_DISPLAY;
static EGLConfig config;
static EGLContext context = EGL_NO_CONTEXT;
static EGLSurface surface = EGL_NO_SURFACE, parking = EGL_NO_SURFACE;
static ANativeWindow* window;
static void* gles_library;

static void* get_proc(const char* name) {
    void* p = gles_library ? dlsym(gles_library, name) : NULL;
    return p ? p : (void*)eglGetProcAddress(name);
}

static void present(void) {
    if (surface != EGL_NO_SURFACE) eglSwapBuffers(display, surface);
}

/* Takes the window the app gave last; without one, waits for it (the app
 * is in the background) while the world stands still. */
static void take_window(void) {
    pthread_mutex_lock(&lock);
    while (!next_window && !quit_requested) {
        if (window_changed && surface != EGL_NO_SURFACE) break;
        struct timespec until;
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_nsec += 100 * 1000000L;
        if (until.tv_nsec >= 1000000000L) until.tv_sec++, until.tv_nsec -= 1000000000L;
        pthread_cond_timedwait(&changed, &lock, &until);
    }
    ANativeWindow* given = next_window;
    int was_changed = window_changed;
    next_window = NULL;
    window_changed = 0;
    pthread_mutex_unlock(&lock);
    if (!was_changed && !given) return;
    if (surface != EGL_NO_SURFACE) {
        eglMakeCurrent(display, parking, parking, context);
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }
    if (window) ANativeWindow_release(window);
    window = given;
    if (window) {
        surface = eglCreateWindowSurface(display, config, window, NULL);
        if (surface != EGL_NO_SURFACE) eglMakeCurrent(display, surface, surface, context);
    }
}

/* ---- Touches and keys, for the game's own screens ---- */

#define QUEUE 64
static struct { int kind, id; float x, y; } queue[QUEUE]; /* kind: 0 down, 1 move, 2 up, 3 key */
static int queued;
static PlatformInput input;
static int frame_w, frame_h;

static void take_input(void) {
    /* Fingers that left last frame are gone; the rest are no longer new. */
    int n = 0;
    for (int i = 0; i < input.touch_count; i++)
        if (!input.touches[i].released) {
            input.touches[n] = input.touches[i];
            input.touches[n].pressed = 0;
            n++;
        }
    input.touch_count = n;
    input.key_count = 0;
    pthread_mutex_lock(&lock);
    for (int q = 0; q < queued; q++) {
        int kind = queue[q].kind, id = queue[q].id;
        if (kind == 3) {
            if (input.key_count < 16) input.keys[input.key_count++] = id;
            continue;
        }
        int t = -1;
        for (int i = 0; i < input.touch_count; i++)
            if (input.touches[i].id == id && !input.touches[i].released) t = i;
        if (kind == 0 && t < 0 && input.touch_count < 8) {
            t = input.touch_count++;
            input.touches[t].id = id;
            input.touches[t].pressed = 1;
            input.touches[t].released = 0;
        }
        if (t < 0) continue;
        input.touches[t].x = queue[q].x * frame_w;
        input.touches[t].y = queue[q].y * frame_h;
        if (kind == 2) input.touches[t].released = 1;
    }
    queued = 0;
    pthread_mutex_unlock(&lock);
}

static void frame(void* io, int* width, int* height) {
    (void)io;
    pthread_mutex_lock(&lock);
    int pending = window_changed;
    pthread_mutex_unlock(&lock);
    if (pending || surface == EGL_NO_SURFACE) take_window();
    /* Without a window (the app was closed meanwhile) the game draws into
     * the parking surface until it sees the request to quit. */
    if (surface == EGL_NO_SURFACE) {
        if (!quit_requested) usleep(30000);
        return;
    }
    EGLint w = 0, h = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    if (w > 0 && h > 0) {
        *width = w;
        *height = h;
        frame_w = w;
        frame_h = h;
    }
    if (blocks_platform && blocks_platform->game_screens) take_input();
}

/* ---- Sound ---- */

static AudioRing* ring;
static AAudioStream* stream;

static aaudio_data_callback_result_t on_audio(AAudioStream* s, void* user, void* data, int32_t frames) {
    (void)s;
    (void)user;
    audio_ring_read(ring, data, frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static AudioRing* audio_ring(void) {
    AAudioStreamBuilder* builder;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) return NULL;
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
    AAudioStreamBuilder_setDataCallback(builder, on_audio, NULL);
    aaudio_result_t opened = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);
    if (opened != AAUDIO_OK) return NULL;
    int rate = AAudioStream_getSampleRate(stream);
    uint32_t capacity = (uint32_t)(rate > 0 ? rate : 48000) / 2;
    ring = calloc(1, 64 + (size_t)capacity * 4);
    ring->version = 1;
    ring->rate = (uint32_t)(rate > 0 ? rate : 48000);
    ring->channels = 2;
    ring->capacity = capacity;
    ring->magic = AUDIO_MAGIC;
    if (AAudioStream_requestStart(stream) != AAUDIO_OK) {
        AAudioStream_close(stream);
        stream = NULL;
        free(ring);
        ring = NULL;
    }
    return ring;
}

/* ---- Calls from the app (io.github.hypocketapp.blocks.NativeGame) ---- */

JNIEXPORT void JNICALL Java_io_github_hypocketapp_blocks_NativeGame_setSurface(JNIEnv* env, jclass cls, jobject s) {
    (void)cls;
    ANativeWindow* w = s ? ANativeWindow_fromSurface(env, s) : NULL;
    pthread_mutex_lock(&lock);
    if (next_window) ANativeWindow_release(next_window);
    next_window = w;
    window_changed = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
}

/* A finger: action 0 down, 1 move, 2 up or cancelled; x and y from 0 to 1 across the view. */
JNIEXPORT void JNICALL Java_io_github_hypocketapp_blocks_NativeGame_touch(JNIEnv* env, jclass cls, jint action, jint id,
                                                                           jfloat x, jfloat y) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    /* Moves of the same finger in a row: only the last one counts. */
    if (action == 1 && queued > 0 && queue[queued - 1].kind == 1 && queue[queued - 1].id == id) queued--;
    if (queued < QUEUE) {
        queue[queued].kind = action;
        queue[queued].id = id;
        queue[queued].x = x;
        queue[queued].y = y;
        queued++;
    }
    pthread_mutex_unlock(&lock);
}

/* A key for the game's screens (BLOCKS_KEY_* in platform.h), such as Back for the menu. */
JNIEXPORT void JNICALL Java_io_github_hypocketapp_blocks_NativeGame_key(JNIEnv* env, jclass cls, jint key) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    if (queued < QUEUE) {
        queue[queued].kind = 3;
        queue[queued].id = key;
        queued++;
    }
    pthread_mutex_unlock(&lock);
}

JNIEXPORT void JNICALL Java_io_github_hypocketapp_blocks_NativeGame_stop(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    pthread_mutex_lock(&lock);
    quit_requested = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);
}

static char** strings(JNIEnv* env, jobjectArray array, int* count) {
    int n = array ? (*env)->GetArrayLength(env, array) : 0;
    char** out = calloc((size_t)n + 1, sizeof *out);
    for (int i = 0; i < n; i++) {
        jstring js = (*env)->GetObjectArrayElement(env, array, i);
        const char* c = (*env)->GetStringUTFChars(env, js, NULL);
        out[i] = strdup(c);
        (*env)->ReleaseStringUTFChars(env, js, c);
        (*env)->DeleteLocalRef(env, js);
    }
    *count = n;
    return out;
}

/* Runs the game until it exits; returns its exit code. [args] are the
 * emulated build's arguments after the program's name (width, height, FPS
 * limit), [environment] "NAME=value" lines. Blocking. */
JNIEXPORT jint JNICALL Java_io_github_hypocketapp_blocks_NativeGame_run(JNIEnv* env, jclass cls, jobjectArray args,
                                                                         jobjectArray environment, jstring log_path) {
    (void)cls;
    const char* log = (*env)->GetStringUTFChars(env, log_path, NULL);
    if (freopen(log, "w", stdout)) setvbuf(stdout, NULL, _IOLBF, 0);
    (*env)->ReleaseStringUTFChars(env, log_path, log);

    int env_count;
    char** vars = strings(env, environment, &env_count);
    for (int i = 0; i < env_count; i++) {
        char* eq = strchr(vars[i], '=');
        if (!eq) continue;
        *eq = 0;
        setenv(vars[i], eq + 1, 1);
    }
    int arg_count;
    char** given = strings(env, args, &arg_count);
    char** argv = calloc((size_t)arg_count + 2, sizeof *argv);
    argv[0] = "blocks";
    for (int i = 0; i < arg_count; i++) argv[i + 1] = given[i];

    gles_library = dlopen("libGLESv3.so", RTLD_NOW | RTLD_LOCAL);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(display, NULL, NULL) || !eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("blocks: no EGL\n");
        return 1;
    }
    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                              EGL_OPENGL_ES3_BIT_KHR, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                              EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLint count = 0;
    if (!eglChooseConfig(display, attribs, &config, 1, &count) || count < 1) {
        printf("blocks: no OpenGL ES 3 config\n");
        return 1;
    }
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);
    const EGLint park_size[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    parking = eglCreatePbufferSurface(display, config, park_size);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, parking, parking, context)) {
        printf("blocks: no OpenGL ES 3 context (0x%x)\n", eglGetError());
        return 1;
    }
    take_window(); /* the first window, before the first frame */
    int w = 0, h = 0;
    frame(NULL, &w, &h);
    if (w > 0 && h > 0 && arg_count >= 2) { /* draw at the window's real size */
        static char ws[16], hs[16];
        snprintf(ws, sizeof ws, "%d", w);
        snprintf(hs, sizeof hs, "%d", h);
        argv[1] = ws;
        argv[2] = hs;
    }

    static BlocksPlatform platform;
    platform.gles = 1;
    platform.get_proc = get_proc;
    platform.present = present;
    platform.frame = frame;
    platform.audio_ring = audio_ring;
    /* The game's own screens, with touch controls, instead of the app's. */
    const char* screens = getenv("HYPOCKET_BLOCKS_SCREENS");
    if (screens && screens[0] == '1') {
        platform.game_screens = 1;
        input.touch = 1;
        input.pointer_x = input.pointer_y = -1;
        platform.input = &input;
    }
    blocks_platform = &platform;
    int code = blocks_main(arg_count + 1, argv);

    if (stream) {
        AAudioStream_requestStop(stream);
        AAudioStream_close(stream);
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    eglDestroySurface(display, parking);
    eglDestroyContext(display, context);
    if (window) ANativeWindow_release(window);
    fflush(stdout);
    __android_log_print(ANDROID_LOG_INFO, "HyPocketBlocks", "game exited with %d", code);
    return code;
}
