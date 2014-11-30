#! /bin/sh
$XGETTEXT `find . src -name "*.cpp" -o -name "*.h" | grep -v "/tests"` -o $podir/libkalarmcal5.pot
