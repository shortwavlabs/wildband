#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Stephane P. Pericat
# RECEIVE checks for the BUILT plugin (docs/p1/PLUGIN-RECEIVE.md): starts the local test server with a connection log,
# points the plugin's stations at it (WILDBAND_TEST_STATIONS), maps a MIDI controller to RECEIVE
# (WILDBAND_TEST_RECEIVE_CC: a mapping, not an activation), runs the headless checks (RX-10..RX-25) and auval (RX-21),
# and counts every connection. The AU must be installed for this user (~/Library/Audio/Plug-Ins/Components).
#   tools/run_plugin_lifecycle_check.sh [build-dir]
#   tools/run_plugin_lifecycle_check.sh --control /path/WILDBAND.vst3
#       positive control: the RX-11 scenario against an older build (and whatever AU is installed); expects connections
set -e
cd "$(dirname "$0")/.."
CONTROL=""
[ "$1" = "--control" ] && { CONTROL="$2"; shift 2; }
BUILD="${1:-build}"
TMP="$(mktemp -d)"
LOG="$TMP/server.log"
trap 'kill "$SERVER" 2>/dev/null; rm -rf "$TMP"' EXIT
ffmpeg -v error -y -f lavfi -i "sine=frequency=440:duration=20" -filter:a "volume=0.5" -ac 2 -ar 44100 -b:a 128k "$TMP/a.mp3"
python3 -I tools/stream_test_server.py "$TMP/a.mp3" 8765 --log "$LOG" &
SERVER=$!
sleep 1
export WILDBAND_TEST_STATIONS="http://127.0.0.1:8765/live,http://127.0.0.1:8765/a,http://127.0.0.1:8765/silent,http://127.0.0.1:8765/c,http://127.0.0.1:8765/live,http://127.0.0.1:8765/a"
export WILDBAND_TEST_RECEIVE_CC=102
: > "$LOG"
STATUS=0
if [ -n "$CONTROL" ]; then
    "$BUILD/wildband_plugin_check_artefacts/Release/wildband_plugin_check" "vst3=$CONTROL" "lifecycle=$LOG" control=1 2>&1 | grep -v "^JUCE"
    exit $?
fi
"$BUILD/wildband_plugin_check_artefacts/Release/wildband_plugin_check" "vst3=$PWD/$BUILD/WILDBAND_Plugin_artefacts/Release/VST3/WILDBAND.vst3" "lifecycle=$LOG" "fixtures=$PWD/tests/fixtures/pre-receive-state" 2>&1 | grep -v "^JUCE" || STATUS=1

echo "AudioUnit validation (auval), RX-21: it runs no message loop, so the plugin's timer never fires there; zero below proves nothing about the network (PR #17 positive control)"
: > "$LOG"
auval -v aumu Wldb SwLb > "$TMP/auval.txt" 2>&1 &
AUVAL=$!
REMOTE=0
while kill -0 "$AUVAL" 2>/dev/null; do
    # lsof does not see connections made through macOS's user-space networking stack; netstat -anv does, with the pid
    N=$(netstat -anv -p tcp 2>/dev/null | grep ":$AUVAL " | grep -v "127.0.0.1" | grep -c "ESTABLISHED\|SYN_SENT" || true)
    [ "$N" -gt "$REMOTE" ] && REMOTE=$N
    sleep 0.2
done
wait "$AUVAL" && AUVAL_OK=1 || AUVAL_OK=0
OPENS=$(grep -c "^open" "$LOG" || true)
grep -q "AU VALIDATION SUCCEEDED" "$TMP/auval.txt" && echo "  pass  RX-21 auval: AU VALIDATION SUCCEEDED" || { echo "  FAIL  RX-21 auval did not succeed"; STATUS=1; }
[ "$OPENS" -eq 0 ] && echo "  pass  RX-21 auval opened $OPENS connections to the counting server" || { echo "  FAIL  RX-21 auval opened $OPENS connections"; STATUS=1; }
[ "$REMOTE" -eq 0 ] && echo "  pass  RX-21 no remote TCP connection seen from the auval process (netstat, sampled every 0.2 s)" || { echo "  FAIL  RX-21 $REMOTE remote TCP connection(s) seen from auval"; STATUS=1; }
exit $STATUS
