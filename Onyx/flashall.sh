#!/bin/sh
for b in 0 1 34; do
    for id in 1 2 3 4 5 6 7 8 9 10 11 12 13 14; do
        bld/obj/setreg -q -b $b $id:19:01
    done
done
