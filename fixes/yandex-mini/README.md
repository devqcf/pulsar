# Yandex Station Mini (1st gen) — pulsar "no sound" fix

Device-local fix for total silence on a Yandex Station Mini (first generation) running
[pulsar](https://github.com/Morvex885/pulsar) v0.5.3. No sound came out at all — neither via
Home Assistant / Music Assistant (Sendspin) nor via the voice assistant (TTS announcements) —
even though the whole software path reported success (`played out`, `0 xruns`, correct delay).

Three independent bugs combined into total silence. Fixes 1 and 2 are device-side workarounds;
fix 3 is a Music Assistant setting.

---

## Bug 1 — amplifier gate collapses during playback

On this Amlogic firmware the TDM driver resets the speaker-amplifier gate
(`/sys/class/tdm_state/amp_table`, `/sys/class/tdm_state/aux_amp`) to `0` the moment a playback
stream is (re)triggered, muting the amp even though the ALSA sink keeps writing samples.
pulsar v0.5.3 arms the gate only *before* opening the PCM and once more *after* opening it; the
trigger happens later, when the first DMA write occurs, so the gate is already `0` while audio flows.

Monitor during a real HA announce:

```
idle:     amp_table=1 spk_mute=hi  pcm=closed
playback: amp_table=0 spk_mute=lo  pcm state: RUNNING
```

Kernel log: `spk_mute_set: mute flag = 1` is logged on every "tdm playback enable".

**Fix:** `amp_keep.sh` — a small, idempotent, self-managing keep-alive that re-arms the gate
(`amp_table=1`, `aux_amp=1`, `SPK mute` on) continuously (every 100 ms, writing only when the value
has collapsed). It is silent on its own (no PCM data = no sound) and exits when pulsar stops.
Wired into `boot.sh` (guarded), so it survives reboots and pulsar restarts.

> Proper upstream fix: re-arm the gate *after* the stream is triggered (RUNNING), not only
> before/after `open()`. See `pulsar-alsa::enable_output` and `sink.rs::Io::open`.

## Bug 2 — Music Assistant streams 44100 Hz, but the Mini dmix is pinned to 48000 Hz

The sendspin sink opens the `mini` PCM with `rate_exact: true`. Music Assistant sends FLAC at the
source rate (44100 Hz), but the `minidmix` dmix is hard-locked to `rate 48000`. The plug device fails
to negotiate:

```
ALSA 'mini' hw_params failed for 44100 Hz/2 ch/16 bit: No such file or directory   (ENXIO)
```

All audio is discarded -> music is silent.

**Fix:** make the `mini` plug resample to 48000 (`asound.conf`).

## Bug 3 — Music Assistant forces 0% volume (feedback loop)

The MA player for the station had
`volume_control = media_player.yandexioreceiver_ff98f02920eee05859efb47d`, pointing at the **dead**
Yandex stock receiver (the stock quasar stack is stopped in pulsar mode). MA could not read or set
the volume through that entity, defaulted to 0, sent `Volume(0)` to the station, the station applied
gain=0 and reported 0 back — a feedback loop stuck at 0%.

**Fix:** remove `volume_control` from the station's MA player settings (`ma_settings_patch.py`), so
MA uses its own internal volume control. Restart MA afterwards.

---

## Files

| File | Purpose |
|---|---|
| `amp_keep.sh` | Amplifier-gate keep-alive. Install to `/data/pulsar/amp_keep.sh`, `chmod +x`. |
| `asound.conf` | Full working `/data/pulsar/asound.conf` (`mini` plug forces 48000). |
| `asound.conf.patch` | The actual change vs. the stock file. |
| `boot.sh.patch` | Adds the guarded `amp_keep.sh` auto-start to `/data/pulsar/boot.sh`. |
| `state.conf.patch` | Bumps volume `0.1` -> `0.5` (0.1 was too quiet on this unit). |
| `ma_settings_patch.py` | Removes the dead `volume_control` override from Music Assistant. |
| `quasar-restore.sh` | Rollback: return the station to the stock Yandex (quasar) stack and remove pulsar's boot autostart. |

## Installation

1. Stop pulsar: `/data/pulsar/ctl.sh disable`
2. Copy `amp_keep.sh` -> `/data/pulsar/amp_keep.sh` and `chmod +x` it.
3. Copy `asound.conf` -> `/data/pulsar/asound.conf` (back up the original first).
4. Apply `boot.sh.patch` to `/data/pulsar/boot.sh` (adds the guarded auto-start before `exec setsid`).
5. Apply `state.conf.patch` to `/data/pulsar/state.conf` (`volume2=0.5`, `volume=0.5`, `muted=0`).
6. Start pulsar: `/data/pulsar/ctl.sh enable` — it restarts in ~1 s via `supervise.sh`;
   `amp_keep.sh` starts with it.
7. Music Assistant: run `ma_settings_patch.py` against the running container and restart MA:
   `docker exec music-assistant-server python3 ma_settings_patch.py && docker restart music-assistant-server`

Verify on the device:

```sh
cat /sys/class/tdm_state/amp_table        # must be 1
pidof amp_keep.sh                          # must be running
grep -E '^volume|^muted' /data/pulsar/state.conf
```

## Rollback — back to the stock Yandex (quasar) stack

`quasar-restore.sh` undoes everything pulsar-related and returns the station to the stock
Yandex assistant. It:

1. stops the amplifier keep-alive (`amp_keep.sh`);
2. stops pulsar and its supervisor (via `/data/pulsar/ctl.sh disable`);
3. removes pulsar's boot autostart (`/etc/init.d/S99zpulsar`) — but keeps a dropbear/SSH line
   inside it if an older installer put one there (SSH autostart can also live in the separate
   `/etc/init.d/S99zdropbear`, which is never touched);
4. ensures quasar is running now and at boot (re-creates the `S99onboot.sh` symlink if missing,
   then `q start`).

```sh
/data/pulsar/quasar-restore.sh            # rollback, keep pulsar files
/data/pulsar/quasar-restore.sh --dry-run  # show what would be done, change nothing
/data/pulsar/quasar-restore.sh --purge    # rollback AND delete /data/pulsar
```

Without `--purge`, pulsar is only disabled: `/data/pulsar/ctl.sh enable` brings it back.
Repeat the note: this turns **off** the pulsar/ESPHome/Music-Assistant path and turns the stock
Yandex assistant **on** again.

## Known limitations / separate bugs

- **HA ESPHome integration `KeyError: <MediaPlayerState.ANNOUNCING: 4>`** — HA 2026.10's ESPHome
  integration doesn't know the `ANNOUNCING` state (value 4) that pulsar v0.5.3 reports, so
  `media_player.<station>` shows `unavailable`. Fix: update the HA ESPHome integration. Does not
  affect audio.
- **Proper upstream fix needs a pulsar rebuild** — see Bug 1/Bug 2 notes above.

## Context

- Station: Yandex Station Mini 1st gen, `axg_s420_v03`, dt-id `axg_s420_v03`,
  firmware `3.211.369.4.24.4711907614.20260928`
- pulsar v0.5.3 (Morvex885/pulsar), unmodified binary
- Home Assistant 2026.10.4, Music Assistant 2.10.8
