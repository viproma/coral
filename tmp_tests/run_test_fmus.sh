#!/bin/sh

# This script is designed to be run under Cygwin, hence the use of "mintty" to
# run commands in new terminal windows.  The script must be run from the
# directory in which it is located.

BUILD="../build32"
FMUSDK="../../fmusdk"
CONFIG="Debug"
mintty $BUILD/src/broker/$CONFIG/broker &
mintty $BUILD/src/dsbexec/$CONFIG/dsbexec short_run.info test_fmus.info tcp://localhost:51390 &
mintty $BUILD/src/slave/$CONFIG/slave 1 tcp://localhost:51391 tcp://localhost:51392 tcp://localhost:51393 "$FMUSDK/fmu10/fmu/cs/testOutput.fmu" testOutput.csv &
$BUILD/src/slave/$CONFIG/slave 2 tcp://localhost:51391 tcp://localhost:51392 tcp://localhost:51393 "$FMUSDK/fmu10/fmu/cs/testInput.fmu" testInput.csv
