#!/usr/bin/env bash
# Test script for Zygote
set -eu

cd "$(dirname "$0")"
make
set -- [0-9][0-9]-*/{main,libcode.$so}

main=$1; shift

hr="################################################################################"
progress() {
    printf >&2 "### %s ${hr:0:$((80 - ${#1} - 5))}\n" "$1"
}

progress "zygote: Launching..."
LD_LIBRARY_PATH=${main%/*}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
DYLD_LIBRARY_PATH=${main%/*}${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH} \
    "$main" data.txt >out.actual &
trap "echo >&2; progress 'zygote: Shutting down...'; kill -TERM $!" EXIT

# wait until it enters zygote
let i=1; until [ -e zygote.socket -o $i -gt 10 ]; do sleep 0.1; let ++i; done
[ -e zygote.socket ] || exit 2

! [ -e out.expected ] ||
    diff -Nu out.expected out.actual
cat out.actual
progress "zygote: OK"
echo >&2

for code; do
    tc=${code%/*}
    progress "$tc: Testing..."
    (set -x; grow zygote.socket "$code" 123) | tee "$tc"/out.actual
    [ -e "$tc"/out.expected ] || continue
    diff -Nu "$tc"/out.expected "$tc"/out.actual
    progress "$tc: OK"
    echo >&2
done
