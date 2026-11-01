#!/bin/sh
# quasar-restore.sh - return a Yandex Station to the STOCK Yandex (quasar) stack.
#
# It does four things:
#   1. stops the amplifier keep-alive (amp_keep.sh) if pulsar started one;
#   2. stops pulsar and its supervisor (via /data/pulsar/ctl.sh disable);
#   3. removes pulsar's boot autostart (/etc/init.d/S99zpulsar);
#   4. makes sure the stock quasar stack is running now and will autostart at boot.
#
# It NEVER touches dropbear/SSH autostart (that is /etc/init.d/S99zdropbear, a
# separate file).
#
# Usage:
#   quasar-restore.sh [-n] [-p]
#     -n, --dry-run   print what would be done, change nothing
#     -p, --purge     also delete /data/pulsar (pulsar binary, configs, state, the
#                     keep-alive). Without -p pulsar stays installed, just disabled:
#                     `/data/pulsar/ctl.sh enable` brings it back.
#   -h, --help        this text
#
# Safe to run repeatedly. After it, press the station's button / use the Yandex app
# as usual; the stock assistant is back.

set -u

D=${PULSAR_DIR:-/data/pulsar}
Q=${PULSAR_Q:-/system/vendor/quasar/q}
AUTOSTART=${PULSAR_AUTOSTART:-/etc/init.d/S99zpulsar}
ONBOOT=/etc/init.d/S99onboot.sh
ROOTKEEPER=/data/quasar/rootkeeper.sh

DRY=0
PURGE=0

die() { echo "quasar-restore: $*" >&2; exit 1; }

for arg in "$@"; do
  case "$arg" in
    -n|--dry-run) DRY=1 ;;
    -p|--purge)   PURGE=1 ;;
    -h|--help)    sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown argument: $arg (try --help)" ;;
  esac
done

# run() executes a command, or just prints it in dry-run mode.
run() {
  if [ "$DRY" = 1 ]; then
    echo "  [dry-run] $*"
  else
    "$@"
  fi
}

say() { echo "== $*"; }

[ "$(id -u)" = 0 ] || die "must run as root"
[ "$PURGE" = 1 ] && [ "$DRY" = 0 ] && \
  echo "quasar-restore: --purge will delete $D" >&2

current_mode() { [ -f "$D/enabled" ] && echo pulsar || echo yandex; }

say "before: $(current_mode) mode | pulsar pid: $(pidof pulsar 2>/dev/null) | quasar pid: $(pidof quasar_launcher2 2>/dev/null)"

# --- 1. stop the amplifier keep-alive -----------------------------------------
say "stopping amp_keep.sh"
pk="$D/.amp_keep.pid"
if [ -f "$pk" ]; then
  p=$(cat "$pk" 2>/dev/null)
  if [ -n "${p:-}" ] && kill -0 "$p" 2>/dev/null && grep -q amp_keep "/proc/$p/cmdline" 2>/dev/null; then
    run sh -c "kill $p 2>/dev/null"
    [ "$DRY" = 1 ] || sleep 0.3
  fi
  run rm -f "$pk"
fi
# belt and suspenders: any amp_keep.sh still alive
for p in $(pidof amp_keep.sh 2>/dev/null); do
  run sh -c "kill $p 2>/dev/null"
done

# --- 2. stop pulsar (and get quasar started) ----------------------------------
say "disabling pulsar"
if [ -x "$D/ctl.sh" ]; then
  run "$D/ctl.sh" disable
else
  # ctl.sh is missing: do the minimum by hand.
  echo "  note: $D/ctl.sh not found, stopping pulsar by hand"
  run sh -c 'rm -f "'"$D"'/enabled"'
  run killall supervise.sh
  run killall pulsar
  [ "$DRY" = 1 ] || sleep 2
  run killall -9 pulsar
  run killall -9 supervise.sh
fi

# --- 3. remove pulsar's boot autostart, keep a dropbear hook if present -------
say "removing boot autostart ($AUTOSTART)"
if [ -f "$AUTOSTART" ]; then
  if grep -q dropbear "$AUTOSTART" 2>/dev/null; then
    echo "  $AUTOSTART also carries a dropbear hook - keeping only that line"
    if [ "$DRY" = 1 ]; then
      echo "  [dry-run] rewrite $AUTOSTART keeping dropbear lines"
    else
      tmp="$AUTOSTART.tmp.$$"
      {
        echo '#!/bin/sh'
        echo '# pulsar autostart removed by quasar-restore; dropbear hook kept.'
        echo '[ "$1" = start ] || exit 0'
        grep dropbear "$AUTOSTART"
      } > "$tmp" && mv "$tmp" "$AUTOSTART"
    fi
  else
    run rm -f "$AUTOSTART"
  fi
else
  echo "  already absent"
fi

# older installers also put a pulsar hook into rootkeeper.sh
if [ -f "$ROOTKEEPER" ] && grep -q pulsar "$ROOTKEEPER" 2>/dev/null; then
  echo "  removing pulsar line(s) from $ROOTKEEPER"
  if [ "$DRY" = 1 ]; then
    echo "  [dry-run] sed -i '/pulsar/d' $ROOTKEEPER"
  else
    tmp="$ROOTKEEPER.tmp.$$"
    grep -v pulsar "$ROOTKEEPER" > "$tmp" && mv "$tmp" "$ROOTKEEPER"
  fi
fi

# make sure the stock quasar autostart link is intact
if [ ! -e "$ONBOOT" ]; then
  say "restoring quasar autostart link ($ONBOOT)"
  run ln -sf ../../system/vendor/quasar/onboot.sh "$ONBOOT"
fi

# --- 4. start quasar now ------------------------------------------------------
say "starting the Yandex (quasar) stack"
if [ "$(pidof quasar_launcher2 2>/dev/null)" ]; then
  echo "  quasar already running"
else
  run "$Q" start
fi

# --- optional: purge ----------------------------------------------------------
if [ "$PURGE" = 1 ]; then
  say "purging $D"
  run rm -rf "$D"
fi

# --- report -------------------------------------------------------------------
say "done"
echo "  mode now: $(current_mode)"
echo "  pulsar pid: $(pidof pulsar 2>/dev/null)"
echo "  quasar pid: $(pidof quasar_launcher2 2>/dev/null)  maind: $(pidof maind 2>/dev/null)"
echo "  autostart: $([ -f "$AUTOSTART" ] && echo "present ($AUTOSTART)" || echo removed)"
if [ "$PURGE" = 1 ] && [ "$DRY" = 0 ]; then
  echo "  pulsar files: deleted"
else
  echo "  pulsar files: kept - re-enable with $D/ctl.sh enable"
fi
