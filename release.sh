#!/bin/sh
ver=1.0.4
mkdir tinyionice-$ver
cp -v main.c Makefile COPYING README.md tinyionice-$ver/
tar zcvf tinyionice-$ver.tar.gz tinyionice-$ver/
rm -r tinyionice-$ver/
