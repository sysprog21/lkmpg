#!/usr/bin/env bash

function do_cppcheck()
{
    local SOURCES=$(find $(git rev-parse --show-toplevel) | grep -E "\.(cpp|cc|c|h)\$")

    local CPPCHECK=$(which cppcheck)
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

    local OPTS="
            --enable=warning,performance,information
            --suppress=unusedFunction:hello-1.c
            --suppress=missingIncludeSystem
            --std=c89 "

    $CPPCHECK $OPTS --xml ${SOURCES} 2> cppcheck.xml
    local ERROR_COUNT=$(cat cppcheck.xml | grep -E -c "</error>" )

    if [ $ERROR_COUNT -gt 0 ]; then
        echo "Cppcheck failed: $ERROR_COUNT error(s)"
        cat cppcheck.xml
        exit 1
    fi
}

function do_sparse()
{
    git clone git://git.kernel.org/pub/scm/devel/sparse/sparse.git --depth=1
    if [ $? -ne 0 ]; then
        echo "Failed to download sparse."
        exit 1
    fi
    pushd sparse
    make sparse || exit 1
    sudo make INST_PROGRAMS=sparse PREFIX=/usr install || exit 1
    popd
    local SPARSE=$(which sparse)

    make -C examples C=2 CHECK="$SPARSE" 2> sparse.log

    local WARNING_COUNT=$(cat sparse.log | grep -E -c " warning:" )
    local ERROR_COUNT=$(cat sparse.log | grep -E -c " error:" )
    local COUNT=`expr $WARNING_COUNT + $ERROR_COUNT`
    if [ $COUNT -gt 0 ]; then
        echo "Sparse failed: $WARNING_COUNT warning(s), $ERROR_COUNT error(s)"
        cat sparse.log
        exit 1
    fi
    make -C examples clean
}

function do_gcc()
{
    local GCC=$(which gcc)
    if [ $? -ne 0 ]; then
        echo "[!] gcc is not installed. Failed to run static analysis with GCC." >&2
        exit 1
    fi

    make -C examples CONFIG_STATUS_CHECK_GCC=y STATUS_CHECK_GCC=$GCC 2> gcc.log

    local WARNING_COUNT=$(cat gcc.log | grep -E -c " warning:" )
    local ERROR_COUNT=$(cat gcc.log | grep -E -c " error:" )
    local COUNT=`expr $WARNING_COUNT + $ERROR_COUNT`
    if [ $COUNT -gt 0 ]; then
        echo "gcc failed: $WARNING_COUNT warning(s), $ERROR_COUNT error(s)"
        cat gcc.log
        exit 1
    fi
    make -C examples CONFIG_STATUS_CHECK_GCC=y STATUS_CHECK_GCC=$GCC clean
}

function do_smatch()
{
    git clone https://github.com/error27/smatch.git --depth=1
    if [ $? -ne 0 ]; then
        echo "Failed to download smatch."
        exit 1
    fi
    pushd smatch
    make smatch || exit 1
    local SMATCH=$(pwd)/smatch
    popd

    make -C examples C=2 CHECK="$SMATCH -p=kernel" > smatch.log
    local WARNING_COUNT=$(cat smatch.log | egrep -c " warn:" )
    local ERROR_COUNT=$(cat smatch.log | egrep -c " error:" )
    local COUNT=`expr $WARNING_COUNT + $ERROR_COUNT`
    if [ $COUNT -gt 0 ]; then
        echo "Smatch failed: $WARNING_COUNT warning(s), $ERROR_COUNT error(s)"
        cat smatch.log | grep "warn:\|error:"
        exit 1
    fi
    make -C examples clean
}

do_cppcheck
do_sparse
do_gcc
do_smatch
exit 0
