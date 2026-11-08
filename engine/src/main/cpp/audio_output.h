// Audio bridge: plays the sound a guest program writes into a shared ring of
// samples, through AAudio. See audio_output.cpp for the file layout.
#pragma once

#include <memory>
#include <string>

namespace hypocket {

struct AudioState;

// Creates the ring file at path and starts playing from it. Returns null if
// no audio stream could be opened.
std::shared_ptr<AudioState>* AudioOutputCreate(const char* path);

// What happened so far, for the test report.
std::string AudioOutputInfo(std::shared_ptr<AudioState>* handle);

void AudioOutputDestroy(std::shared_ptr<AudioState>* handle);

}  // namespace hypocket
