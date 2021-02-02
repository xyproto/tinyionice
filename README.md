# ion

Standalone fork of `ionice` from [util-linux](https://github.com/karelzak/util-linux).

The source code in `util-linux` have different licenses in different source files.

This fork is only GPL2 licensed.

The localization is removed, and support for older systems is removed, but other than that it's a fully featured version of `ionice` in just 350 lines of C.

Builds to an 18k executable with GCC 10:

    gcc -std=c18 -Ofast -flto -pipe -fPIC -fno-plt -fstack-protector-strong -D_GNU_SOURCE main.c -Wl,-flto -o ion

Install on Linux:

    sudo install -Dm755 ion /usr/bin/ion

Install on FreeBSD:

    doas install -Dm755 ion /usr/local/bin/ion

Version: 1.0.0
