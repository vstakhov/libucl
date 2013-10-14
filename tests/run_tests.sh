#!/bin/sh

TESTS="1 2 3"

if [ $# -lt 1 ] ; then
	echo 'Specify binary to run as the first argument'
	exit 1
fi

for _t in $TESTS ; do
	$1 ${TEST_DIR}/$_t.in ${TEST_DIR}/$_t.out
	if [ $? -ne 0 ] ; then
		rm ${TEST_DIR}/$_t.out
		echo "Test: $_t failed"
		exit 1
	fi
	if [ -f ${TEST_DIR}/$_t.res ] ; then
	diff -s ${TEST_DIR}/$_t.out ${TEST_DIR}/$_t.res -u 2>/dev/null
		if [ $? -ne 0 ] ; then
			rm ${TEST_DIR}/$_t.out
			echo "Test: $_t output missmatch"
			exit 1
		fi
	fi
done
