set -e
. ./config.sh
cd ./libc/

export PREFIX=${CROSS_PATH:-/usr/}
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

make clean
make install
