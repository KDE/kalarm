#!/bin/sh

kickoffrcname=`qtpaths --locate-file  GenericConfigLocation kickoffrc`
if [ -f "$kickoffrcname" ]; then
   sed -i "s/\/kalarm.desktop/\/org.kde.kalarm.desktop/" $kickoffrcname
fi
