#!/bin/sh

sed -i "s/\/kalarm.desktop/\/org.kde.kalarm.desktop/" `kf5-config --path config --locate kickoffrc`
