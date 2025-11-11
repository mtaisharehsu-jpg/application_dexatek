#/bin/sh
unset LD_LIBRARY_PATH

source ./../build/toolchain.env

make dexatek

cp ./dexatek/main_application/dexatek_main ~/nfs/