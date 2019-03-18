#!/bin/sh
QMAKE=
[ -n "$(which qmake 2> /dev/null)" ] && QMAKE=qmake
[ -n "$(which qmake-qt4 2> /dev/null)" ] && QMAKE=qmake-qt4
[ -z "$QMAKE" ] && echo "This sample requires qmake to build" && exit 1
$QMAKE || exit $?
make || exit $?
