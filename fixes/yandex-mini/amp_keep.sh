#!/bin/sh
# Yandex Station Mini (first gen): amplifier-gate keep-alive.
#
# The Amlogic TDM driver resets /sys/class/tdm_state/amp_table (and aux_amp) to 0
# the moment a playback stream is (re)triggered, muting the speaker amp even though
# the ALSA sink keeps writing samples (0 xruns). This holds the gate armed.
#
# Silent on its own (no PCM data = no sound). Single instance, self-cleaning.
D=/data/pulsar
PIDF="$D/.amp_keep.pid"
export LD_LIBRARY_PATH=/system/vendor/quasar/libs
AMX=/system/workdir/bin/amixer

# Single instance: only bail out if the PID in the file is a live amp_keep.sh.
if [ -f "$PIDF" ]; then
    p=$(cat "$PIDF" 2>/dev/null)
    if [ -n "$p" ] && kill -0 "$p" 2>/dev/null &&
       grep -q amp_keep "/proc/$p/cmdline" 2>/dev/null; then
        exit 0
    fi
fi
echo $$ > "$PIDF"
trap '[ "$(cat "$PIDF" 2>/dev/null)" = "$$" ] && rm -f "$PIDF"' EXIT

while [ -f "$D/enabled" ] && pidof pulsar >/dev/null 2>&1; do
    a=$(cat /sys/class/tdm_state/amp_table 2>/dev/null)
    [ "$a" = 1 ] || { echo 1 > /sys/class/tdm_state/amp_table 2>/dev/null; $AMX sset "SPK mute" on >/dev/null 2>&1; }
    b=$(cat /sys/class/tdm_state/aux_amp 2>/dev/null)
    [ "$b" = 1 ] || echo 1 > /sys/class/tdm_state/aux_amp 2>/dev/null
    sleep 0.1
done
