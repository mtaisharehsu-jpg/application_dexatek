SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../)
include $(SDKSRC_DIR)/application_dexatek/internal.mk
include $(realpath $(CURDIR))/AppRootfs.mk
include $(realpath $(CURDIR))/modbus.mk
include $(realpath $(CURDIR))/mdns.mk
include $(realpath $(CURDIR))/usbhid.mk
include $(realpath $(CURDIR))/project_config.mk

# specify build flags
MAKEFLAGS += -j4

# specify build tools
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

# CFLAGS
CFLAGS := -g3 -O0 -std=gnu99 -MMD -MP -lm -lpthread
CFLAGS += -DCONFIG_PLATFORM_LINUX=1 -DREMOVE_AGTX_BOOL -DEPOLL #-DCONFIG_USE_OPENSSL=1
CFLAGS += $(MODBUS_CFLAGS) $(MDNS_CFLAGS)

# Library CFLAGS (includes -fPIC for shared library)
LIB_CFLAGS := $(CFLAGS) -fPIC

# paths
root = $(realpath $(CURDIR))
SYSTEM_BIN := $(root)/bin
INSTALL_PATH := $(SYSROOT_DBG)/usr/dexatek

# install application files
INSTALL_TARGET_FOLDER := $(USRDATAFS_PATH)
INSTALL_TARGET_FOLDERS := $(addprefix $(INSTALL_TARGET_FOLDER)/,$(APP_ROOTFS_FOLDERS))
INSTALL_TARGET_SUFFIX := $(subst $(APP_ROOTFS_PATH), ,$(APP_ROOTFS_FILES))
INSTALL_TARGET_FILES := $(addprefix $(INSTALL_TARGET_FOLDER),$(INSTALL_TARGET_SUFFIX))

# specify include directories
INCS := $(root)/../
INCS := $(root)/../../
INCS += $(root)/include
INCS += $(MODBUS_INCS)
INCS += $(HID_INCS)

INCS += $(LIBGPIO_PATH)

LIBS :=
LIBS += $(LIBGPIO_PATH)

# LDFLAGS
LDFLAGS :=
LDFLAGS += $(MODBUS_LDFLAGS)
LDFLAGS += $(HID_LDFLAGS)
LDFLAGS += $(MDNS_LDFLAGS)
LDFLAGS += -lgcc_s

# Library LDFLAGS (excludes static libraries that aren't PIC-compiled)
LIB_LDFLAGS :=
LIB_LDFLAGS += $(MODBUS_LDFLAGS)
LIB_LDFLAGS += $(HID_LDFLAGS)
LIB_LDFLAGS += $(MDNS_LDFLAGS)
LIB_LDFLAGS += -lgcc_s

# GPIO object files for library (compiled with -fPIC)
GPIO_LIB_OBJS := $(LIBGPIO_PATH)/gpio.lib.o

# specify source files
SRCS :=
SRCS += $(wildcard $(root)/*.c)
# SRCS += $(wildcard $(root)/managers/wifi_manager/*.c)
SRCS += $(wildcard $(root)/managers/ethernet_manager/*.c)
SRCS += $(wildcard $(root)/services/mdns_service/*.c)
SRCS += $(wildcard $(root)/services/sntp_service/*.c)
SRCS += $(wildcard $(root)/utilities/*.c)
SRCS += $(wildcard $(root)/drivers/*.c)
SRCS += $(MODBUS_SRCS)
SRCS += $(MDNS_SRCS)
SRCS += $(HID_SRCS)

SRCS += $(wildcard $(root)/services/redfish/src/*.c)

# [DON'T TOUCH] calculate corresponding object files and auto-dependencies 
OBJS = $(SRCS:.c=.o)
LIB_OBJS = $(SRCS:.c=.lib.o)
DEPS = $(SRCS:.c=.d)

# specify bin
BIN = dexatek_main

# specify library
LIB = libdexatek.so

# sepcify targets
.DEFAULT_GOAL := default

.PHONY: default
default: all

.PHONY: all
all: $(root)/$(BIN)

.PHONY: lib
lib: $(root)/$(LIB)

$(root)/$(BIN): $(OBJS)
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(addprefix -L,$(LIBS)) $(LDFLAGS) -lgpio

$(root)/$(LIB): $(LIB_OBJS)
	$(Q)$(CC) -shared $(LIB_CFLAGS) $^ -o $@ $(LIB_LDFLAGS)

.PHONY: install
install: $(SYSTEM_BIN)/$(BIN)
	$(Q) mkdir -p $(INSTALL_PATH)
	$(Q) cp $(BIN) $(INSTALL_PATH)
	$(Q) mkdir -p $(INSTALL_TARGET_FOLDERS)
	$(Q) cp -rf $(APP_ROOTFS_PATH)/* $(INSTALL_TARGET_FOLDER)

.PHONY: lib-install
lib-install: $(root)/$(LIB)
	$(Q) mkdir -p $(SYSROOT_DBG)/usr/lib
	$(Q) cp $(LIB) $(SYSROOT_DBG)/usr/lib
	
$(SYSTEM_BIN)/$(BIN): $(root)/$(BIN) | $(SYSTEM_BIN)
	$(Q)install -m 777 $< $(dir $@)

$(SYSTEM_BIN) :
	$(Q)mkdir -p $@

.PHONY: uninstall
uninstall:
	$(Q)rm -rf $(SYSTEM_BIN)
	$(Q)if [ -d $(SYSTEMFS)/bin ]; then \
		rmdir --ignore-fail-on-non-empty $(SYSTEMFS)/bin; \
	fi
	$(Q)rm -f $(INSTALL_TARGET_FILES)

.PHONY: lib-uninstall
lib-uninstall:
	$(Q)rm -f $(SYSROOT_DBG)/usr/lib/$(LIB)

.PHONY: clean
clean:
	$(Q)rm -f $(root)/$(BIN)
	$(Q)find $(root) -name "*.[ado]" -exec rm -f {} \;

.PHONY: lib-clean
lib-clean:
	$(Q)find $(root) -name "*.lib.o" -exec rm -f {} \;

# general directory independent targets
%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(addprefix -D,$(DEFS)) $< -c -o $@ $(addprefix -I,$(INCS)) $(addprefix -L,$(LIBS))

%.lib.o: %.c
	$(Q)$(CC) $(LIB_CFLAGS) $(addprefix -D,$(DEFS)) $< -c -o $@ $(addprefix -I,$(INCS)) $(addprefix -L,$(LIBS)) 

# Autodependencies
-include $(DEPS)
