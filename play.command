#!/bin/bash
cd /Users/csullivan/Desktop/cs/C++

if g++ -std=c++17 main.cpp -o game -I/Users/csullivan/.brew/include -L/Users/csullivan/.brew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo; then
    nohup ./game >/dev/null 2>&1 &
    sleep 0.2
    osascript -e 'tell application "Terminal" to close (every window whose name contains "play.command")' &
fi
