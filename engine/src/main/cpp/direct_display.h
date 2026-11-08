#ifndef HYPOCKET_DIRECT_DISPLAY_H
#define HYPOCKET_DIRECT_DISPLAY_H

#include <android/native_window.h>

#include <cstddef>
#include <memory>

namespace hypocket {

struct State;

// Takes ownership of window. Returns null when direct display is not
// possible (Android 9, or an error); socket_name receives the abstract socket
// name the VirGL server must connect to (HYPOCKET_DISPLAY_SOCKET).
std::shared_ptr<State>* DirectDisplayCreate(ANativeWindow* window, char* socket_name, size_t name_size);
void DirectDisplayResize(std::shared_ptr<State>* handle, int width, int height);
int DirectDisplayShownFrames(std::shared_ptr<State>* handle);
void DirectDisplayDestroy(std::shared_ptr<State>* handle);

}  // namespace hypocket

#endif
