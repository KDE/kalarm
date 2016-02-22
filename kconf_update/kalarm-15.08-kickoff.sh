#!/bin/sh

kickoffrcname=`kf5-config --path config --locate kickoffrc`
if [ -f "$kickoffrcname" ]; then
   sed -i "s/\/kalarm.desktop/\/org.kde.kalarm.desktop/" $kickoffrcname
fi
