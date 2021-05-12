# Tiny I/O Nice

This is a fork of `ionice` from [util-linux](https://github.com/karelzak/util-linux), in just 324 lines of C.

* The support for obsolete systems is removed.
* The translated output messages are removed.
* Other than that it's a fully featured drop-in replacement of `ionice`.
* The resulting executable is 13k when built with GCC 10 on Linux. It can also be built with [`cxx tiny`](https://github.com/xyproto/cxx).
* `util-linux` uses several open source licenses across many different files. This fork is only based on files licensed under GPL2. `tinyionice` is only GPL2 licensed.

## Build

    gcc -O2 -fPIC -fstack-protector-strong -D_GNU_SOURCE -s -z norelro main.c -o tinyionice

## Install

    sudo install -Dm755 tinyionice /usr/bin/tinyionice

## General info

* Version: 1.0.0
* License: GPL2
