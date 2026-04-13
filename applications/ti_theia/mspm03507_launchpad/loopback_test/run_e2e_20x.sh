#!/bin/bash
# Run main track and service track E2E tests 20 times each, sequentially.
# Usage: ./run_e2e_20x.sh <cmd-port> <dec-port>
#   e.g. ./run_e2e_20x.sh /dev/cu.usbmodem4 /dev/cu.usbmodemMG3500011

CMD_PORT="${1:?Usage: $0 <cmd-port> <dec-port>}"
DEC_PORT="${2:?Usage: $0 <cmd-port> <dec-port>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RUNS=20

main_pass=0
main_fail=0
svc_pass=0
svc_fail=0

echo "========================================"
echo "E2E Loopback Test Suite — ${RUNS} iterations"
echo "CMD: ${CMD_PORT}  DEC: ${DEC_PORT}"
echo "========================================"

for i in $(seq 1 $RUNS); do
    echo ""
    echo "--- Iteration $i / $RUNS ---"

    # Main track
    result=$(python3 -m pytest "$SCRIPT_DIR/dcc_main_track_test.py" \
        --cmd-port "$CMD_PORT" --dec-port "$DEC_PORT" -q 2>&1 | tail -1)
    echo "  Main track:    $result"
    if echo "$result" | grep -q "passed" && ! echo "$result" | grep -q "failed"; then
        main_pass=$((main_pass + 1))
    else
        main_fail=$((main_fail + 1))
    fi

    # Service track
    result=$(python3 -m pytest "$SCRIPT_DIR/dcc_service_track_test.py" \
        --cmd-port "$CMD_PORT" --dec-port "$DEC_PORT" -q 2>&1 | tail -1)
    echo "  Service track: $result"
    if echo "$result" | grep -q "passed" && ! echo "$result" | grep -q "failed"; then
        svc_pass=$((svc_pass + 1))
    else
        svc_fail=$((svc_fail + 1))
    fi
done

echo ""
echo "========================================"
echo "SUMMARY (${RUNS} iterations)"
echo "========================================"
echo "Main track:    ${main_pass} passed, ${main_fail} failed"
echo "Service track: ${svc_pass} passed, ${svc_fail} failed"
echo "========================================"
