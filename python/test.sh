#!/bin/sh
set -xe
python setup.py build_ext --inplace
./test_uclmodule.py -v
rm -rfv build
rm ucl.so
