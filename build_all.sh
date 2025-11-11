#/bin/sh

source ./../build/toolchain.env

make library-clean
make dexatek-clean

make library
make dexatek

# cp ./dexatek/main_application/dexatek_main ~/nfs/