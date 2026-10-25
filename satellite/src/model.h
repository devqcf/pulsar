// Per-model hardware description of the supported Yandex Stations. The model is detected once
// from the devicetree ("yandex,<codename>" in /proc/device-tree/compatible), with the Mini's
// board id (/proc/device-tree/amlogic-dt-id = axg_s420_v03) as a fallback.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cs {

struct Model {
  std::string codename;       // devicetree codename: "cucumber", "plum"
  std::string host_prefix;    // ESPHome node name prefix, the MAC suffix is appended
  std::string friendly_name;  // shown in Home Assistant / Music Assistant
  std::string model_name;

  // Capture: interleaved samples from capture_pcm, capture_bits per channel (S16_LE or S32_LE);
  // VQE gets mic_channels / spk_channels. For S32_LE, capture_shift low bits are discarded when
  // reducing to the VQE's 16-bit input: 16 for a full 32-bit sample, 8 for the Mini's stream,
  // whose 24 significant bits are right-justified.
  std::string capture_pcm;
  unsigned capture_rate = 0, capture_channels = 0, capture_bits = 16, capture_shift = 16;
  std::vector<unsigned> mic_channels, spk_channels;
  std::string vqe_preset;

  // LED ring (+ optional logo), each either an Nx1 BGR framebuffer behind an ledfb trigger with
  // per-channel sysfs LEDs "<led_prefix>:<color>:<ring|logo><index>" as fallback, or an IS31FL323x
  // controller on an i2c bus ("i2c_dev" non-empty; 18 channels = 6 RGB pixels).
  struct LedStrip {
    int count = 0;
    int first_index = 0;      // sysfs numbering starts at 0 (Midi) or 1 (Lite 2)
    std::string fb_dev;       // "/dev/fb0"
    std::string fb_name;      // expected /sys/class/graphics/fbN/name
    std::string trigger;      // LED trigger that routes the framebuffer to the LEDs
    std::string sysfs_kind;   // "ring" / "logo"
    std::string i2c_dev;      // "/dev/i2c-0"; empty: framebuffer/sysfs
    int i2c_addr = 0;
  };
  std::string led_prefix;
  LedStrip ring, logo;        // logo.count == 0: no logo

  // 4-digit 7-segment clock: "<clock_dir><segment><position>" sysfs LEDs.
  std::string clock_dir;      // e.g. "/sys/class/leds/cucumber:white:clock-"
  char clock_seg_base = 'a';  // name of segment a ('a' or 'A')
  int clock_pos_base = 1;     // name of the leftmost digit position
  std::vector<std::string> clock_colon, clock_aux;  // colon LEDs; LEDs kept dark
  std::string clock_center;  // LED at g height between digits 1 and 2 ("" = none)
  // true: segments switch 0/255, dimming via the driver chips' "current" attribute;
  // false: dimming via the per-segment brightness value itself.
  bool clock_chip_current = true;

  std::string keys_input, mute_input;  // evdev device names; empty: absent
  int action_key = 582;                // KEY_VOICECOMMAND; 0: no action button
  int mute_key = 0;                    // KEY_MUTE on a keypad without a SW_MUTE_DEVICE switch
  bool has_light_sensor = true;        // ltr308als01 IIO ambient light sensor
  std::string error_sound;             // relative to /system/vendor/quasar/sounds/
  // GitHub owner/repo for the daemon's release check. The Mini must not take upstream armhf
  // binaries: they target glibc 2.30, not the Mini's 2.23 (and do not know its hardware).
  std::string update_repo = "Morvex885/pulsar";

  // GStreamer alsasink device for playback (e.g. "speakers" or "hw:0,0"). No ALSA softvol is
  // exposed on every model, so the master volume can instead be applied to the player channels.
  std::string playback_sink = "speakers";
  bool software_volume = false;
  // ALSA playback switch (on hw:0) that must be 1 (on) for the amplifier to pass audio. It can
  // be left off after a playback PCM closes, so switch it on before starting playback. Empty: no
  // such control.
  std::string spk_mute_ctl;
  // Sysfs switches that gate the speaker amplifier. They are written to "1" before a playback
  // PCM is opened; the Mini needs /sys/class/tdm_state/amp_table and .../aux_amp. Empty: no gate.
  std::vector<std::string> amp_enable_paths;

  // Stock volume curve: platform level -> raw value (0..255) of the softvol "Master Playback
  // Volume" on the speakers PCM, piecewise linear between the points; the first point is level 0
  // (raw 0), the last one the top level. quasar.cfg volume_capability.volume_manager_settings
  // .platformToAlsaMap; the platform level steps by 5 % (changeStepPercent).
  struct VolumePoint {
    int level, raw;
  };
  std::vector<VolumePoint> volume_map;

  // Zigbee radio: Telink co-processor on a UART; its reset and boot lines are GPIO value files
  // (active low, 1 = asserted) exported by the kernel's zb_control driver. zigbee_tty empty: none.
  std::string zigbee_tty;
  std::string zigbee_reset, zigbee_boot;
};

// The detected model; nullptr on unsupported hardware.
const Model *detect_model();

}  // namespace cs
