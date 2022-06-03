# ![tinyionice](img/tinyionice.png) Tiny I/O Nice

This is a fork of `ionice` from [util-linux](https://github.com/karelzak/util-linux), in just 300 lines of C.

* Support for obsolete systems is removed.
* The translated output messages are removed.
* Other than that it's a fully featured drop-in replacement for `ionice`.
* The resulting executable is only 11k when built with GCC 11 on Linux (and then stripped with `sstrip`).
* `util-linux` uses several open source licenses across many different files. This fork is only based on files licensed under GPL2. `tinyionice` is only GPL2 licensed.


## Build

### With gcc

    gcc -O2 -fPIC -fstack-protector-strong -D_GNU_SOURCE -s -z norelro main.c -o tinyionice

### With [cxx](https://github.com/xyproto/cxx) and gcc

    cxx tiny

### With `make`

    make

## Install

As `root`, with `sudo` or `doas`:

### Manually

    sudo install -Dm755 tinyionice /usr/bin/tinyionice

### With `make`

    make install

## License

The original source code of `ionice` is either GPL2 licensed:

```
Copyright (C) 2004 Jens Axboe <jens@axboe.dk>
Released under the terms of the GNU General Public License version 2
```

And also released under `No copyright is claimed. This code is in the public domain; do with it what you wish`:

```
Copyright (C) 2010 Karel Zak <kzak@redhat.com>
Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
No copyright is claimed. This code is in the public domain; do with it what you wish.
```

I wish to relicense all code under these terms as GPL2.

The changes made in 2021 and beyond are:

```
Copyright (C) 2021-2022 Alexander F. Rødseth <xyproto@archlinux.org>
Released under the terms of the GNU General Public License version 2
```

The entire code of `tinyionice` is now released under GPL2.

The full license text is in the `COPYING` file.


## General info

* Version: 1.0.1
* License: GPL2
