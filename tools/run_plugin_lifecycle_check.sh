#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Stephane P. Pericat
# Receiver lifecycle checks for the BUILT plugin (docs/p1/PLUGIN-RECEIVER-LIFECYCLE.md): starts the local test server
# with a connection log, points the plugin's stations at it (WILDBAND_TEST_STATIONS), runs the headless lifecycle
# checks (RL-10..RL-17) and auval (RL-18), and counts every connection. The AU must be installed for this user
# (~/Library/Audio/Plug-Ins/Components) for the AU part and auval. Usage: tools/run_plugin_lifecycle_check.sh [build-dir]
set -e
cd "$(dirname "$0")/.."
BUILD="${1:-build}"
TMP="$(mktemp -d)"
LOG="$TMP/server.log"
trap 'kill "$SERVER" 2>/dev/null; rm -rf "$TMP"' EXIT
ffmpeg -v error -y -f lavfi -i "sine=frequency=440:duration=20" -filter:a "volume=0.5" -ac 2 -ar 44100 -b:a 128k "$TMP/a.mp3"
python3 -I tools/stream_test_server.py "$TMP/a.mp3" 8765 --log "$LOG" &
SERVER=$!
sleep 1
export WILDBAND_TEST_STATIONS="http://127.0.0.1:8765/live,http://127.0.0.1:8765/a,http://127.0.0.1:8765/silent,http://127.0.0.1:8765/c,http://127.0.0.1:8765/live,http://127.0.0.1:8765/a"
: > "$LOG"
STATUS=0
"$BUILD/wildband_plugin_check_artefacts/Release/wildband_plugin_check" "vst3=$PWD/$BUILD/WILDBAND_Plugin_artefacts/Release/VST3/WILDBAND.vst3" "lifecycle=$LOG" 2>&1 | grep -v "^JUCE" || STATUS=1

echo "AudioUnit validation (auval), RL-18"
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
grep -q "AU VALIDATION SUCCEEDED" "$TMP/auval.txt" && echo "  pass  RL-19 auval: AU VALIDATION SUCCEEDED" || { echo "  FAIL  RL-19 auval did not succeed"; STATUS=1; }
[ "$OPENS" -eq 0 ] && echo "  pass  RL-18 auval opened $OPENS connections to the counting server" || { echo "  FAIL  RL-18 auval opened $OPENS connections"; STATUS=1; }
[ "$REMOTE" -eq 0 ] && echo "  pass  RL-18 no remote TCP connection seen from the auval process (netstat, sampled every 0.2 s)" || { echo "  FAIL  RL-18 $REMOTE remote TCP connection(s) seen from auval"; STATUS=1; }
exit $STATUS
