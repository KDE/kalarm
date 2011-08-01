#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.kcfg -o -name \*.ui` >> rc.cpp || exit 11
$XGETTEXT `find . \( ! -path "./kdepim-runtime/*" \) -a \( -name "*.cpp" -o -name "*.h" \)` -o $podir/akonadi_kalarm_resource.pot
rm -f rc.cpp
