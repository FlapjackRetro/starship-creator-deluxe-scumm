#!/bin/bash
cd /tmp/claude-0/sv; pkill -9 scummvm; ./sess.sh stop; pkill Xvfb; sleep 1; DBG=${DBG:-1} ./sess.sh start $EXTRA; sleep 45; ./sess.sh click 270 411; sleep 2; ./sess.sh click 636 421; sleep 2; ./sess.sh click 108 405; sleep 15; ./sess.sh click 370 155; sleep 3; ./sess.sh click 470 215; sleep 4; ./sess.sh shot g1
