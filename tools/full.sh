#!/bin/bash
cd /tmp/claude-0/sv; ./flow.sh >/dev/null 2>&1
./sess.sh click 290 160; sleep 3; ./sess.sh click 337 215; sleep 2; ./sess.sh click 108 417; sleep 10
./sess.sh click 108 417; sleep 8; ./sess.sh click 690 417; sleep 8
./sess.sh click 285 365; ./sess.sh type Enterprise; sleep 1; ./sess.sh click 435 365; sleep 3; ./sess.sh click 550 365; sleep 4; ./sess.sh shot h1
./sess.sh click 690 417; sleep 15; ./sess.sh shot h2; ./sess.sh log 8
