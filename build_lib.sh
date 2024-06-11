set -e
. ./config.sh
cd ./libc/
make clean
make install
