#!/usr/bin/env bash
# Test script for Zygote
set -eu

main=$1; shift

hr="################################################################################"
progress() {
    printf >&2 "### %s ${hr:0:$((80 - ${#1} - 5))}\n" "$1"
}

progress "zygote: Launching..."
LD_LIBRARY_PATH=${main%/*}:$LD_LIBRARY_PATH \
    "$main" data.txt >out.actual &
trap "progress 'zygote: Shutting down...'; kill -TERM $!" EXIT
sleep 1; cat out.actual
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

progress "zygote: Verifying output..."
! [ -e out.expected ] ||
    diff -Nu out.expected out.actual
progress "zygote: PASS"
