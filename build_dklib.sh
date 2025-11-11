#/bin/sh
unset LD_LIBRARY_PATH

source ./../build/toolchain.env

make dexatek-lib

cp ./dexatek/main_application/libdexatek.so ~/nfs/