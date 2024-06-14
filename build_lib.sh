set -e
. ./config.sh
cd ./libc/

export PREFIX=/usr/local
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

x86_64-elf-gcc --print-search-dirs

make clean
make install
