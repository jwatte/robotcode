#!/bin/sh
trap 'bld/obj/spower noservos' EXIT SIGINT
bld/obj/setservo -t 500 1:2600 2:2550 3:2550 4:1500 5:1530 6:1530 7:1500 8:1530 9:1530 10:2600 11:2550 12:2550
