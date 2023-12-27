#!/bin/bash

# kill a process and its children
kill_children() {
    local parent_pid=$1
    local children=$(pgrep -P $parent_pid)

    for child_pid in $children; do
        kill_children $child_pid
    done

    kill -9 $parent_pid
}

if [ $# -eq 1 ]; then
    parent_pid=$1
    kill_children $parent_pid
    echo "kill $parent_pid and its children"
else
    echo "PID needed as argument"
    exit 1
fi
