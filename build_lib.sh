set -e
. ./config.sh

cd ../mlibc/
#Build mlibc
meson setup --wipe --cross-file ../nightos-meson-target.txt --prefix="$PREFIX" -Dheaders_only=true build
meson build
cd ./build/
ninja
ninja install

cd ../../libc/

make clean
make install-libs

#cd ../
meson setup --wipe --cross-file ../nightos-meson-target.txt --prefix="$PREFIX" -Dheaders_only=false -Ddefault_library=both build
meson build
cd ./build/
ninja -v
ninja install