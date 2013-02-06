#!/usr/bin/env bash
set -eu

: ${N:=100000} ${M:=20}
generate-input() {
    echo $(($N * $M))
    for i in $(seq $M)
    do seq $N &
    done | shuf
}

Here=$(dirname "$0")
Here=$(cd "$Here" && pwd -P)
cd "$Here"

: ${PREFIX:=$Here/../@prefix@}
export PATH="$PREFIX/bin:$PATH"
case $(uname) in
    Darwin)
        export DYLD_LIBRARY_PATH="$PREFIX/lib:${DYLD_LIBRARY_PATH:-}"
        sharedflag=-dynamiclib
        ;;
    *)
        export LD_LIBRARY_PATH="$PREFIX/lib:${LD_LIBRARY_PATH:-}"
        sharedflag=-shared
        ;;
esac
export CFLAGS="-I$PREFIX/include ${CFLAGS:-}"
export LDFLAGS="-L$PREFIX/lib ${LDFLAGS:-}"
export LIBS="-lzygote ${LIBS:-}"

hr() {
echo
echo ------------------------------------------------------------------------
echo
}

# generate input
echo Generating Input...
trap "rm -f input_file" EXIT
generate-input >input_file
set +e

hr

## without libzygote
(
read -p "Press Enter to proceed with The Naive Way..."
set -x
#cc -Wall -o example -DZYGOTE_DISABLED example.c -ldl -rdynamic
cc -Wall -o example example-naive.c

set +e
time ./example input_file 23 4.56
time ./example input_file 78 9.01
time ./example input_file 90 1.23
time ./example input_file 23 4.56
time ./example input_file 54 3.21
)

hr

## with libzygote
(
read -p "Press Enter to proceed with Smarter Way..."
set -x
cc -Wall -o example-zygote  $CFLAGS -fPIC  example.c  $LDFLAGS $LIBS
cc -Wall -o example-run.so  $CFLAGS -fPIC  example.c  $LDFLAGS $sharedflag $LIBS

./example-zygote input_file &
zygote_pid=$!
# wait until zygote process finishes loading
until [ -e zygote.socket ]; do sleep .1; done 2>/dev/null

set +e
time grow zygote.socket example-run.so 23 4.56
time grow zygote.socket example-run.so 78 9.01
time grow zygote.socket example-run.so 90 1.23
time grow zygote.socket example-run.so 23 4.56
time grow zygote.socket example-run.so 54 3.21

kill $zygote_pid
)
