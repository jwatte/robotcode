#!/bin/sh
trap 'bld/obj/off' EXIT SIGINT
bld/obj/setservo --maxtorque 200 14:2048 13:2048 1:2700 2:1400 3:1400 4:1400 5:2700 6:2700 7:1400 8:2700 9:2700 10:2700 11:1400 12:1400
