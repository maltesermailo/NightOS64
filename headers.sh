#!/bin/sh
set -xe
. ./config.sh
 
mkdir -p "$SYSROOT"
 
for PROJECT in $SYSTEM_HEADER_PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install-headers)
done

#Build mlibc
cd ./mlibc/
meson setup --cross-file ../nightos-meson-target.txt --prefix="$PREFIX" -Dheaders_only=true build
meson build
cd ./build/
ninja
ninja install

