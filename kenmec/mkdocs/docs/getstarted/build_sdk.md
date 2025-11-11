# How to build SDK

## Untar the SDK file

```bash
tar zxvf sdk-xxx.tar.gz
```

## Build SDK

Change directory to SDK top:

```bash
cd $SDK_TOP
```

then type following commands:

```bash
make toolchain
make kenmec
```

After some time, if you see the following output, it means the SDK is built successfully.

```bash
- Building boot image
- Copy uImage
- Installing ICE scripts to PKG
make -C /home/kellen/work/projects/kenmec/augentix-platform-application_sdk/programmer/ice_script install
- Building NVS programmer
CC      nvspc.o
CC      nvsp_mach.o
CC      nvspc_flash.o
AS      nvspc_entry.o
LD      nvspc.elf
OBJCOPY nvspc.bin
HEXDUMP nvspc.hex
OBJDUMP nvspc.asm
- Installing NVS programmer to PKG
- Copy Filesystem to PKG
Copying filesystem to /home/kellen/work/projects/kenmec/augentix-platform-application_sdk/build/output
- Padding Root FileSystem
Make finished at Wed 01 Oct 2025 02:07:15 PM CST
make[1]: Leaving directory '/home/kellen/work/projects/kenmec/augentix-platform-application_sdk/build'
```

## Output

After `make kenmec`, the SDK will be generated in $SDK_TOP/output/image.swu

```bash
$SDK_TOP/output/image.swu
```
