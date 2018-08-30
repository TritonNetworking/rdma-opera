#!/usr/bin/env bash

cd "$(dirname "$0")"

# Check if MPI environment is loaded
if ! [ -x "$(command -v mpirun)" ]; then
    source ./setup-hpcx.sh
fi

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
    -d|--debug)
    DEBUG=true
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
    rm -rf *
    if [ $DEBUG ]; then
        cmake -DCMAKE_BUILD_TYPE=Debug ../src
    else
        cmake -DCMAKE_BUILD_TYPE=Release ../src
    fi
fi

if [ $CLEAN ]; then
    make clean
fi

make
)

