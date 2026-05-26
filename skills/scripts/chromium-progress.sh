#!/usr/bin/env bash
# chromium-progress.sh — Live Chromium build progress via .o file counting
#
# Installed at ~/.hermes/scripts/chromium-progress.sh
# Used with: cronjob action=create --script chromium-progress.sh --no-agent --schedule "every 15m"
#
# Design: does NOT invoke `ninja` (would conflict with the running build).
# Instead counts .o files via find, which is fast and non-blocking.
set -e

# Match any chromium container regardless of --name
CONTAINER=$(docker ps --filter name=chromium --format '{{.ID}}' 2>/dev/null)

if [ -n "$CONTAINER" ]; then
    CHROOT="/checkout/src/out/Default"
    O_DONE=$(docker exec "$CONTAINER" sh -c "find $CHROOT -name '*.o' 2>/dev/null | wc -l" 2>/dev/null || echo "?")
    STARTED=$(docker inspect "$CONTAINER" --format '{{.State.StartedAt}}' 2>/dev/null | date -f - +%s 2>/dev/null)
    NOW=$(date +%s)
    ELAPSED=$(( (NOW - STARTED) / 60 ))
    RAM=$(docker stats --no-stream "$CONTAINER" --format '{{.MemUsage}}' 2>/dev/null || echo "?")
    echo "📦 ${O_DONE} .o files | RAM: ${RAM} | running ${ELAPSED}m"
    exit 0
fi

# No container — try the build log as fallback
LOG="$HOME/chromium-android/build-logs/build_v2.log"
if [ -f "$LOG" ]; then
    LAST=$(grep -oP '\[\K[0-9]+/[0-9]+(?=\])' "$LOG" 2>/dev/null | tail -1)
    if [ -n "$LAST" ]; then
        CUR="${LAST%%/*}"
        TOTAL="${LAST##*/}"
        PCT=$(awk "BEGIN {printf \"%.1f\", $CUR/$TOTAL*100}")
        echo "📦 [$CUR/$TOTAL] ($PCT%) from log — container finished"
        exit 0
    fi
fi

echo "⏳ Chromium build — no active container or log found."
