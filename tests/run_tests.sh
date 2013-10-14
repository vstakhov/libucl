#!/bin/sh

if [ $# -lt 1 ] ; then
	echo 'Specify binary to run as the first argument'
	exit 1
fi


for _tin in ${TEST_DIR}/*.in ; do
	_t=`echo $_tin | sed -e 's/.in$//'`
	$1 $_t.in $_t.out
	if [ $? -ne 0 ] ; then
		echo "Test: $_t failed, output:"
		cat $_t.out
		rm $_t.out
		exit 1
	fi
	if [ -f $_t.res ] ; then
	diff -s $_t.out $_t.res -u 2>/dev/null
		if [ $? -ne 0 ] ; then
			rm $_t.out
			echo "Test: $_t output missmatch"
			exit 1
		fi
	fi
	rm $_t.out
done
