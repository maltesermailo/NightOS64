set -e
. ./config.sh
cd ./libc/

export PREFIX=/usr/local/cross
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

echo $PATH
echo $GITHUB_PATH

make clean
make install
