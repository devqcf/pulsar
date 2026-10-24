#include "hw/spk_mute.h"

#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "core/log.h"

namespace cs {

bool enable_amp(const std::vector<std::string> &sysfs_paths) {
  bool ok = true;
  for (const std::string &path : sysfs_paths) {
    if (path.empty()) continue;
    int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
      LOGW("amp: cannot open '%s': %s", path.c_str(), std::strerror(errno));
      ok = false;
      continue;
    }
    const char value[] = "1\n";
    ssize_t n = ::write(fd, value, sizeof value - 1);
    if (n != ssize_t(sizeof value - 1)) {
      LOGW("amp: cannot write '%s': %s", path.c_str(), n < 0 ? std::strerror(errno) : "short write");
      ok = false;
    } else {
      LOGD("amp: '%s'=1", path.c_str());
    }
    ::close(fd);
  }
  return ok;
}

bool unmute_speaker(const std::string &ctl_name) {
  if (ctl_name.empty()) return true;
  snd_ctl_t *ctl = nullptr;
  int rc = snd_ctl_open(&ctl, "hw:0", 0);
  if (rc < 0) {
    LOGW("spk: cannot open hw:0: %s", snd_strerror(rc));
    return false;
  }
  snd_ctl_elem_value_t *val;
  snd_ctl_elem_value_alloca(&val);
  snd_ctl_elem_value_set_interface(val, SND_CTL_ELEM_IFACE_MIXER);
  snd_ctl_elem_value_set_name(val, ctl_name.c_str());
  snd_ctl_elem_value_set_integer(val, 0, 1);
  rc = snd_ctl_elem_write(ctl, val);
  if (rc < 0) LOGW("spk: cannot unmute '%s': %s", ctl_name.c_str(), snd_strerror(rc));
  else LOGD("spk: '%s'=1", ctl_name.c_str());
  snd_ctl_close(ctl);
  return rc >= 0;
}

}  // namespace cs
