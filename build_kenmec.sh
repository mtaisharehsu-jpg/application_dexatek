#/bin/sh
unset LD_LIBRARY_PATH

source ./../build/toolchain.env

make kenmec

cp ./kenmec/main_application/kenmec_main ~/nfs/