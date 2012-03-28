#!/bin/sh

> k5.cfg

while true; do

echo ubique el mouse en 1 ... pulse ENTER
read A
echo -n `xdotool getmouselocation | sed -e 's/x://' -e 's/y://' | awk '{print $1" "$2}'` >> k5.cfg

echo ubique el mouse en 2 ... pulse ENTER
read A
echo -n " "`xdotool getmouselocation | sed -e 's/x://' -e 's/y://' | awk '{print $1" "$2}'` >> k5.cfg

echo escriba la letra minuscula ... pulse ENTER
read A
echo -n " "$A >> k5.cfg

echo escriba la letra mayuscula ... pulse ENTER
read A
echo -n " "$A >> k5.cfg

echo escriba el sonido 1 2 3 o 4 .. pulse ENTER
read A
echo " "$A >> k5.cfg


done

