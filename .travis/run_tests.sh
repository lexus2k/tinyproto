#!/bin/sh

create_terminals() {
    # Create virtual terminals
    socat -d pty,b115200,raw,echo=0,link=/tmp/sideA pty,b115200,raw,echo=0,link=/tmp/sideB &
    sleep 1
}

close_terminals_and_exit() {
    # Terminate terminals
    pkill tiny_loopback
    pkill socat
    exit $1
}

run_fd_test() {
    # Full duplex tests
    # Run sideB in listen/loopback mode
    ./bld/tiny_loopback -p /tmp/sideB -t fd -r -w 7 &
    sleep 0.5
    # Run tests on sideA for 15 seconds
    # We don't want to limit loopback side, so test can miss some frames
    ./bld/tiny_loopback -p /tmp/sideA -t fd -g -r -w 7
    if [ $? -ne 0 ]; then
        close_terminals_and_exit 1
    fi
    pkill tiny_loopback
    sleep 0.5
}

run_light_test() {
    # HD tests
    # Run sideB in listen/loopback mode
    ./bld/tiny_loopback -p /tmp/sideB -t light -r &
    sleep 0.5
    # Run tests on sideA for 15 seconds
    ./bld/tiny_loopback -p /tmp/sideA -t light -g -r
    if [ $? -ne 0 ]; then
        close_terminals_and_exit 1
    fi
    pkill tiny_loopback
}

create_terminals

run_fd_test

run_light_test

close_terminals_and_exit 0
