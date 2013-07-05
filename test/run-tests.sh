#!/usr/bin/env bash
set -eu

Here=$(dirname "$0")
Here=$(cd "$Here" && pwd -P)
cd "$Here"

# setup common test environment
: ${PREFIX:=$Here/../@prefix@}
case $(uname) in
    Darwin)
        export DYLD_LIBRARY_PATH="$PREFIX/lib:${DYLD_LIBRARY_PATH:-}"
        so=dylib
        sharedflag=-dynamiclib
        ;;
    *)
        export LD_LIBRARY_PATH="$PREFIX/lib:${LD_LIBRARY_PATH:-}"
        so=so
        sharedflag=-shared
        ;;
esac
export PREFIX so sharedflag
export PATH="$PREFIX/bin:$PATH"
export CFLAGS="-I$PREFIX/include ${CFLAGS:-}"
export CXXFLAGS="-I$PREFIX/include ${CXXFLAGS:-}"
export LDFLAGS="-L$PREFIX/lib ${LDFLAGS:-}"
export LIBS="-lzygote ${LIBS:-}"

# run tests (given, or all)
[ $# -gt 0 ] || set -- [0-9][0-9]-*/
for t; do
    t=${t%/}
    echo "################################################################################"
    echo "##### $t"
    echo "################################################################################"
    ! [ -t 1 ] || read -n1 -p"Press Enter to proceed... "
    (
    cd "$t"
    ./test.sh
    echo "################################################################################"
    echo
    )
done
