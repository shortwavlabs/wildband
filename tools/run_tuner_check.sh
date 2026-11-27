#!/bin/sh
# Runs the receiver-level tuner tests: makes three short tone clips, starts the local test server, runs
# wildband_tuner_check against it, stops the server. Needs ffmpeg and python3. Usage: tools/run_tuner_check.sh [build-dir]
set -e
cd "$(dirname "$0")/.."
BUILD="${1:-build}"
TMP="$(mktemp -d)"
trap 'kill "$SERVER" 2>/dev/null; rm -rf "$TMP"' EXIT
for pair in a:440 b:880 c:1320; do
    ffmpeg -v error -y -f lavfi -i "sine=frequency=${pair#*:}:duration=20" -filter:a "volume=0.5" -ac 2 -ar 44100 -b:a 128k "$TMP/${pair%%:*}.mp3"
done
python3 -I tools/stream_test_server.py "$TMP/a.mp3" 8765 "$TMP/b.mp3" "$TMP/c.mp3" &
SERVER=$!
sleep 1
"$BUILD/wildband_tuner_check_artefacts/Release/wildband_tuner_check" http://127.0.0.1:8765 http://127.0.0.1:8799/live
