#!/bin/sh
set -e
set -o nounset

if [ -z "$1" ]; then
    echo "usage: test.sh command"
    exit 1
fi
cmd="$1"
for i in data/*.jpg; do
    out=`echo $i | sed -e s@data/@@ -e s@.jpg@.tga@`
    echo "$cmd $i $out"
    $cmd $i $out
done
