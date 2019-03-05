#!/bin/bash

bit64=`php -n -r 'echo PHP_INT_SIZE == 8 ? "1" : "0";'`

if [[ ${bit64} != "1" ]]; then
	export CFLAGS="-m32"
fi

phpize && ./configure --enable-vld-dev && make clean && make all && make install
