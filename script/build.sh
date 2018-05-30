#!/usr/bin/env bash

# Parse command line arguments
# Source: https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -f|--force)
    FORCE=true
    shift # past argument
    ;;
    -c|--clean)
    CLEAN=true
    shift # past argument
    ;;
    *)
    echo "Unrecognized option $key"
    exit 2
esac
done

(#set -x;
mkdir -p ../build
cd ../build

if [ $FORCE ]; then
    rm -r *
    cmake ../src
fi

if [ $CLEAN ]; then
    make clean
fi

make
)

