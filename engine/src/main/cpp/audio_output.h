// Audio bridge: plays the sound a guest program writes into a shared ring of
// samples, through AAudio. See audio_output.cpp for the file layout.
#pragma once

#include <memory>
#include <string>

namespace hypocket {

struct AudioState;

// Creates the ring file at path and starts playing from it. With existing,
// path is shared memory someone else made (it is opened, not created or
// truncated). Returns null if the ring or the audio stream could not be set
// up; AudioOutputLastError then says which step failed and why.
std::shared_ptr<AudioState>* AudioOutputCreate(const char* path, bool existing);

// Why the last AudioOutputCreate returned null, or empty.
std::string AudioOutputLastError();

// What happened so far, for the test report.
std::string AudioOutputInfo(std::shared_ptr<AudioState>* handle);

void AudioOutputDestroy(std::shared_ptr<AudioState>* handle);

}  // namespace hypocket
