# How to build OTA image

## Overview

This document describes how to build OTA image for hub.

## Build

Change directory to SDK top:

```bash
cd $SDK_TOP
```

Build the OTA image commands:

```bash
make kenmec
make ota_gen
```

## Output

After `make ota_gen`, the OTA image will be generated in $SDK_TOP/output/image.swu

```bash
$SDK_TOP/output/image.swu
```

!!! info "See also"
    [How to upgrade hub](upgrade_hub.md) <br>
    [Redfish API: OTA image (WIP)](../redfish/ota_image.md)