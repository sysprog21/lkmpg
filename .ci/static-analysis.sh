#!/usr/bin/env bash

SOURCES=$(find $(git rev-parse --show-toplevel) | egrep "\.(cpp|cc|c|h)\$")

CPPCHECK=$(which cppcheck)
if [ $? -ne 0 ]; then
    echo "[!] cppcheck not installed. Failed to run static analysis the source code." >&2
    exit 1
fi

## Suppression list ##
# This list will explain the detail of suppressed warnings.
# The prototype of the item should be like:
# "- [{file}] {spec}: {reason}"
#
# - [hello-1.c] unusedFunction: False positive of init_module and cleanup_module.
# - [*.c] missingIncludeSystem: Focus on the example code, not the kernel headers.

OPTS="  --enable=warning,style,performance,information
        --suppress=unusedFunction:hello-1.c
        --suppress=missingIncludeSystem
        --std=c89 "

$CPPCHECK $OPTS --xml ${SOURCES} 2> cppcheck.xml
ERROR_COUNT=$(cat cppcheck.xml | egrep -c "</error>" )

if [ $ERROR_COUNT -gt 0 ]; then
    echo "Cppcheck failed: error count is $ERROR_COUNT"
    cat cppcheck.xml
    exit 1
fi
exit 0
