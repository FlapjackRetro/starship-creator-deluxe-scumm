#!/bin/bash
# sess.sh start [scummvm args] | shot <name> | click x y | key K | stop | log
export LD_LIBRARY_PATH=/tmp/claude-0/sv/prefix/lib HOME=/tmp/claude-0/sv/home SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy} DISPLAY=:77
cmd=$1; shift
case $cmd in
 start)
  pkill -x scummvm 2>/dev/null; pkill Xvfb 2>/dev/null; sleep 1
  Xvfb :77 -screen 0 800x600x24 >/dev/null 2>&1 &
  sleep 2; mkdir -p /tmp/claude-0/sv/work; cd /tmp/claude-0/sv/work
  nohup /tmp/claude-0/sv/scummvm/scummvm --start-movie=${MOVIE:-ShipMain/Creator.dxr} -d${DBG:-1} "$@" trekship-win > /tmp/claude-0/sv/live.log 2>&1 &
  echo started;;
 shot) import -window root /tmp/claude-0/sv/$1.png; echo /tmp/claude-0/sv/$1.png;;
 click) python3 -I /tmp/claude-0/sv/xin.py click $1 $2;;
 key) python3 -I /tmp/claude-0/sv/xin.py key $1;;
 type) python3 -I /tmp/claude-0/sv/xin.py type "$1";;
 stop) pkill -x scummvm; pkill Xvfb;;
 log) grep -v "HardwareInput\|STUB: loadConfig\|flash' cast\|kParentScript" /tmp/claude-0/sv/live.log | tail -${1:-20} | cut -c1-220;;
esac
