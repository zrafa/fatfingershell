#!/bin/bash

# For Openmoko Freerunner

export DISPLAY=:0
xrandr -o 3
cd /usr/share/fatfingershell
# remove the line below if you want to load the oss emulation. You need it
# if you want sound
# lsmod | grep oss || (modprobe snd_pcm_oss ; modprobe snd_mixer_oss)
fatfingershell -v -e /bin/sh
echo 0 > /sys/class/leds/neo1973:vibrator/brightness 
# echo 0 > /sys/devices/platform/neo1973-vibrator.0/leds/neo1973:vibrator/brightness
xrandr -o 0

