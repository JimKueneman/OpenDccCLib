#!/bin/bash
# =============================================================================
# check_symbol_leakage.sh — Verify gated-OFF modules export no code symbols.
#
# Usage: check_symbol_leakage.sh <library_file> <prefix1> [prefix2] ...
#
# Scans a static library for DEFINED text symbols (T/t) matching the given
# prefixes. A match means a module that should have been #ifdef'd out by the
# active dcc_user_config.h still compiled real code in — the gating is wrong
# (or a direct cross-module call pulled it in). This is the "define-when-off"
# bug class, complementary to the link-time "use-when-off" check.
#
# On macOS, nm prefixes C symbols with an underscore, so we match both
# " T _Prefix" and " T Prefix".
#
# Exit code: 0 = clean, 1 = leaked symbols found.
# =============================================================================

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <library_file> <prefix1> [prefix2] ..."
    exit 1
fi

LIBRARY="$1"
shift

if [ ! -f "$LIBRARY" ]; then
    echo "FAIL: library not found: $LIBRARY"
    exit 1
fi

LEAKED=0

for PREFIX in "$@"; do

    MATCHES=$(nm "$LIBRARY" 2>/dev/null \
        | grep -E ' [Tt] _?'"$PREFIX" \
        || true)

    if [ -n "$MATCHES" ]; then
        echo "FAIL: symbol leakage for gated-off prefix '$PREFIX' in $(basename "$LIBRARY"):"
        echo "$MATCHES"
        echo ""
        LEAKED=1
    fi

done

if [ $LEAKED -eq 0 ]; then
    echo "PASS: no symbol leakage in $(basename "$LIBRARY") ($*)"
fi

exit $LEAKED
