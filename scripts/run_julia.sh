#!/bin/bash -u

preload_libs="/usr/lib/libtcmalloc.so"

if [ "$#" != "2" ]; then
    echo "usage: $0 [julia-executable] [julia-script]"
    exit
fi

#LD_PRELOAD=$preload_libs $1 --track-allocation=all $2
LD_PRELOAD=$preload_libs $1 -O3 $2
#LD_PRELOAD=$preload_libs $1 $2
