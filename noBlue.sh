#!/bin/bash

# Command to let me enable/disable the BeagleBone board LED's.  At night they can make it hard to sleep
# Created by Jim Powell <LossyHorizon@jep.cc>  Nov 9, 2016

function sub() {
    OLD_VAL=$(cat /sys/class/leds/beaglebone\:green\:usr$1/trigger | grep --only-matching "\[.*\]")
    echo "pin = USB$1, Value was $OLD_VAL, now set to [$2]"
    echo $2 >/sys/class/leds/beaglebone\:green\:usr$1/trigger
}


# User must provide one argument (on/off)
if [[ "$#" -ne 1 ]]; then
    echo "Illegal number of parameters, you must specify on/off"
    exit 6
fi

if [[ ${1^^} = "ON" ]]; then
    echo "Enabling board Lights"
    sub 0 heartbeat
    sub 1 mmc0
    # Older Linux install had this as "cpu0", but that no longer exists
    sub 2 none
    sub 3 mmc1

elif [[ ${1^^} = "OFF" ]]; then
    echo "Disabling board lights"
    sub 0 none
    sub 1 none
    sub 2 none
    sub 3 none

else
    echo "ERROR: unknown option $1"
    exit 6
fi
