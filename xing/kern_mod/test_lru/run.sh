#!/bin/bash

MODULE_NAME=test_lru.ko

set -e

usage() {
	echo "Usage:"
	echo "  $ ./run.sh 1 (Install $MODULE_NAME)"
	echo "  $ ./run.sh 2 (Remove $MODULE_NAME)"
}

if [ "$1" == "1" ]; then
	insmod $MODULE_NAME
elif [ "$1" == "2" ]; then
	rmmod $MODULE_NAME
else
	usage
fi
