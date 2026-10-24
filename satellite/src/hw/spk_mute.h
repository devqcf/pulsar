// Speaker amplifier mute control of the Station Mini.
#pragma once

#include <string>
#include <vector>

namespace cs {

// Writes 1 (playback switch on, i.e. unmuted) to an ALSA boolean control such as the Mini's
// "SPK mute". It can be left off after a playback PCM closes, so call this right before
// opening/starting one. No-op for an empty name. Safe to call from any thread.
bool unmute_speaker(const std::string &ctl_name);

// Writes "1" to each sysfs amplifier gate (the Mini's tdm_state/amp_table and aux_amp). An empty
// list is a no-op. Safe to call from any thread.
bool enable_amp(const std::vector<std::string> &sysfs_paths);

}  // namespace cs
