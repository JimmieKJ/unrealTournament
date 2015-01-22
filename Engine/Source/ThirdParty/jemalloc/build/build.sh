#!/bin/sh

set +x

./configure --with-mangling --with-jemalloc-prefix=je_
make

set -x
