# How to build the application

## Prepare

!!! info "Hint"
    If you already executed the following commands, you can skip it.

```bash
cd $SDK_TOP
make toolchain
make kenmec
```

## Generate environment variables

Change directory to $SDK/build folder:

```bash
cd $SDK_TOP/build
```

Copy the following content, and paste it to the terminal:

```bash
cat <<- EOF > "$PWD/toolchain.env" 
# Augentix Linux SDK
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export SDK_PATH=$PWD/../
export BR2_DL_DIR="$PWD/../buildroot/dl" 
export PATH=$PWD/../toolchain/gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf/bin:$PATH
EOF
```

## Build application

Change directory to $SDK/application_dexatek folder:

```bash
cd $SDK_TOP/application_dexatek
```

Build the application commands:

```bash
./build_kenmec.sh
```

## Output

If build script is successful, the output file will be generated in:

```bash
./kenmec/main_application/kenmec_main
```

The build script will copy the output file to the NFS folder:

```bash
cp ./kenmec/main_application/kenmec_main ~/nfs/
```

!!! info "See also"
    [How to update/replace application](update_application.md)
