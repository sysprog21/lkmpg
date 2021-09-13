#!/usr/bin/env bash

function build_example()
{   
    make -C examples || exit 1
}

function list_mod()
{
    # Filter out the modules specified in non-working
    ls examples/*.ko | awk -F "[/|.]" '{print $2}' | grep -vFxf .ci/non-working
}

function run_mod()
{
    # insert/remove twice to ensure resource allocations
    ( sudo insmod "examples/$1.ko" && sudo rmmod "$1" ) || exit 1
    ( sudo insmod "examples/$1.ko" && sudo rmmod "$1" ) || exit 1
}

function run_examples()
{
    for module in $(list_mod); do
        echo "Running $module"
        run_mod "$module"
    done
}

build_example
run_examples
