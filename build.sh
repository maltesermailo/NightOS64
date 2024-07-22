set -e
. ./config.sh

cd ./init/
make clean
make install

cd ..

make build