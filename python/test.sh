#!/bin/sh
set -xe

PYTHON=python

runtest() {
	which $PYTHON || return 0
	$PYTHON setup.py build_ext --inplace
	$PYTHON test_uclmodule.py -v
	rm -rfv build ucl*.so
}

runtest
PYTHON=python2.6 runtest
PYTHON=python2.7 runtest
PYTHON=python3.2 runtest
PYTHON=python3.3 runtest
PYTHON=python3.4 runtest
PYTHON=python3.5 runtest
