#! /bin/sh
aclocal
libtoolize --install --copy
autoconf
./configure "$@"
