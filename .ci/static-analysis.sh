#!/usr/bin/env bash

function do_cppcheck()
{
    local SOURCES=$(find $(git rev-parse --show-toplevel) | egrep "\.(cpp|cc|c|h)\$")

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
            --enable=warning,style,performance,information
            --suppress=unusedFunction:hello-1.c
            --suppress=missingIncludeSystem
            --std=c89 "

    $CPPCHECK $OPTS --xml ${SOURCES} 2> cppcheck.xml
    local ERROR_COUNT=$(cat cppcheck.xml | egrep -c "</error>" )

    if [ $ERROR_COUNT -gt 0 ]; then
        echo "Cppcheck failed: $ERROR_COUNT error(s)"
        cat cppcheck.xml
        exit 1
    fi
}

function do_sparse()
{
    wget -q http://www.kernel.org/pub/software/devel/sparse/dist/sparse-latest.tar.gz
    if [ $? -ne 0 ]; then
        echo "Failed to download sparse."
        exit 1
    fi
    tar -xzf sparse-latest.tar.gz
    pushd sparse-*/
    make sparse || exit 1
    sudo make INST_PROGRAMS=sparse PREFIX=/usr install || exit 1
    popd
    local SPARSE=$(which sparse)

    make -C examples C=2 CHECK="$SPARSE" 2> sparse.log

    local WARNING_COUNT=$(cat sparse.log | egrep -c " warning:" )
    local ERROR_COUNT=$(cat sparse.log | egrep -c " error:" )
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
    local GCC=$(which gcc-10)
    if [ $? -ne 0 ]; then
        echo "[!] gcc-10 is not installed. Failed to run static analysis with GCC." >&2
        exit 1
    fi

    make -C examples CONFIG_STATUS_CHECK_GCC=y STATUS_CHECK_GCC=$GCC 2> gcc.log

    local WARNING_COUNT=$(cat gcc.log | egrep -c " warning:" )
    local ERROR_COUNT=$(cat gcc.log | egrep -c " error:" )
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
    wget -q https://repo.or.cz/smatch.git/snapshot/refs/heads/master.tar.gz
    if [ $? -ne 0 ]; then
        echo "Failed to download smatch."
        exit 1
    fi
    tar -xzf master.tar.gz
    pushd smatch-master-*
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
