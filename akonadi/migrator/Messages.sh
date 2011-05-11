#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.kcfg -o -name \*.ui` >> rc.cpp || exit 11
$XGETTEXT *.cpp *.h -o $podir/kalarm-migrator.pot
rm -f rc.cpp
