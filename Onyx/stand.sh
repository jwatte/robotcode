#!/bin/sh
trap 'bld/obj/spower noservos' EXIT SIGINT
bld/obj/setservo -t 500 1:2600 2:2650 3:2650 4:1500 5:1430 6:1430 7:1500 8:1430 9:1430 10:2600 11:2650 12:2750
