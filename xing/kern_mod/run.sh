#!/bin/bash

usage() {
	echo "Usage:"
	echo "  $ ./run.sh 1 (Install module)"
	echo "  $ ./run.sh 2 (Remove module)"
}

set -e

MODULE_NAME=async_crossing.ko

if [ "$1" == "1" ]; then
	insmod $MODULE_NAME
elif [ "$1" == "2" ]; then
	rmmod $MODULE_NAME
else
	usage
fi
