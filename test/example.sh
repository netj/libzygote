#!/usr/bin/env bash
set -eu

: ${N:=1000000} ${M:=10}
generate-input() {
    bash -c "
    echo $(($N * $M))
    for i in \$(seq $M)
    do seq $N
    done | shuf
    "
}

Here=$(dirname "$0")
Here=$(cd "$Here" && pwd -P)
cd "$Here"

: ${PREFIX:=$Here/../@prefix@}
PATH="$PREFIX/bin:$PATH"
LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH"
CFLAGS="-I$PREFIX/include ${CFLAGS:-}"
LDFLAGS="-L$PREFIX/lib ${LDFLAGS:-}"
LIBS="-lzygote ${LIBS:-}"

## with libzygote
(
echo Smarter Way
set -x
cc -Wall -o example-zygote  $CFLAGS -fPIC  example.c  $LDFLAGS $LIBS
cc -Wall -o example-run.so  $CFLAGS -fPIC  example.c  $LDFLAGS -shared $LIBS

generate-input | ./example-zygote /dev/stdin &
zygote_pid=$!
until [ -e zygote.socket ]; do echo -n .; sleep 1; done 2>/dev/null
echo

set +e
time grow zygote.socket example-run.so 23 4.56
time grow zygote.socket example-run.so 78 9.01
time grow zygote.socket example-run.so 90 1.23
time grow zygote.socket example-run.so 23 4.56
time grow zygote.socket example-run.so 54 3.21

kill $zygote_pid
)


## without libzygote
(
echo Naive Way
set -x
#cc -Wall -o example -DZYGOTE_DISABLED example.c -ldl -rdynamic
cc -Wall -o example example-orig.c
set +e
generate-input >/tmp/input_file
time ./example /tmp/input_file 23 4.56
time ./example /tmp/input_file 78 9.01
time ./example /tmp/input_file 90 1.23
time ./example /tmp/input_file 23 4.56
time ./example /tmp/input_file 54 3.21
rm -f /tmp/inputfile
)

