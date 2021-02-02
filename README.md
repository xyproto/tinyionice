# ion

Standalone fork of `ionice` from [util-linux](https://github.com/karelzak/util-linux).

* The source code in `util-linux` have different licenses in different source files.
* This fork is only GPL2 licensed.
* The localization is removed, and support for older systems is removed.
* Other than that it's a fully featured version of `ionice` in just 342 lines of C.
* The resulting executable is 18k when built with GCC 10 on Linux. It also build with [cxx](https://github.com/xyproto/cxx).

Build:

    gcc -std=c18 -Ofast -flto -pipe -fPIC -fno-plt -fstack-protector-strong -D_GNU_SOURCE main.c -Wl,-flto -o ion

Install:

    sudo install -Dm755 ion /usr/bin/ion

Version: 1.0.0
