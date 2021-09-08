#!/usr/bin/env bash

function build_example()
{   
    make -C examples || exit 1
}

function list_mod()
{
    #following list will contain all file names which are not specified in file non-working.
    echo `ls examples/*.ko | awk -F "[/|.]" '{print $2}' | grep -vFxf .ci/non-working`
}

#test module 2 times
function run_mod()
{
    ( sudo insmod "examples/$1.ko" && sudo rmmod "$1" ) || exit 1;
    ( sudo insmod "examples/$1.ko" && sudo rmmod "$1" ) || exit 1;
}

function run_examples()
{
    for module in $(list_mod); do
        echo "$module"
        run_mod "$module"
    done
}

build_example
run_examples