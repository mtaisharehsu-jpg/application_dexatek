SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../)
include $(SDKSRC_DIR)/application_dexatek/internal.mk
include $(realpath $(CURDIR))/modbus.mk
include $(realpath $(CURDIR))/mdns.mk
include $(realpath $(CURDIR))/usbhid.mk

# mbedtls
MBEDTLS_VERSION = 2.28.0
MBEDTLS_ROOT = $(BUILDROOT_OUTPUT_BUILD_PATH)/mbedtls-$(MBEDTLS_VERSION)
MBEDTLS_INC = $(MBEDTLS_ROOT)/include
MBEDTLS_LIB = $(MBEDTLS_ROOT)/library

# specify build flags
MAKEFLAGS += -j4

# specify build tools
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

# CFLAGS
CFLAGS := -g3 -O0 -std=gnu99 -MMD -MP -lm -lpthread
CFLAGS += -DCONFIG_PLATFORM_LINUX=1 -DREMOVE_AGTX_BOOL -DEPOLL
CFLAGS += -Wall -Wextra
CFLAGS += $(MODBUS_CFLAGS)
CFLAGS += $(MDNS_CFLAGS)
CFLAGS += $(HID_CFLAGS)

# paths
root = $(realpath $(CURDIR))
SYSTEM_BIN := $(root)/bin
INSTALL_PATH := $(SYSROOT_DBG)/usr/kenmec

# install application files
INSTALL_TARGET_FOLDER := $(USRDATAFS_PATH)

# specify include directories
INCS := $(root)/../
INCS += $(root)/../../
INCS += $(root)/include
INCS += $(MODBUS_INCS)
INCS += $(MDNS_INCS)
INCS += $(HID_INCS)
INCS += $(LIBGPIO_PATH)
INCS += $(MBEDTLS_INC)
INCS += $(SQLITE3_INC)
INCS += $(OPENSSL_INC)
INCS += $(root)/redfish/include

# specify library directories
LIBS := $(root)/../../dexatek/main_application
LIBS += $(MODBUS_LIBS)
LIBS += $(HID_LIBS)
LIBS += $(LIBGPIO_PATH)
LIBS += $(MBEDTLS_LIB)
LIBS += $(SQLITE3_LIB)
LIBS += $(OPENSSL_LIB)

# LDFLAGS
LDFLAGS := -ldexatek $(MODBUS_LDFLAGS) $(MDNS_LDFLAGS) $(HID_LDFLAGS) -lgpio -lgcc_s

# specify source files
SRCS :=
SRCS += $(wildcard $(root)/*.c)
SRCS += $(wildcard $(root)/control_logic/*.c)
SRCS += $(wildcard $(root)/control_logic/ls80/*.c)
SRCS += $(wildcard $(root)/control_logic/lx1400/*.c)
SRCS += $(wildcard $(root)/redfish/src/*.c)
SRCS += $(wildcard $(root)/misc/*.c)

# [DON'T TOUCH] calculate corresponding object files and auto-dependencies 
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)	

# specify bin
BIN = kenmec_main

# specify targets
.DEFAULT_GOAL := default

.PHONY: default
default: all

.PHONY: all
all: $(root)/$(BIN)

$(root)/$(BIN): $(OBJS)
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(addprefix -L,$(LIBS)) $(LDFLAGS) -lmbedtls -lmbedx509 -lmbedcrypto -lsqlite3 -lcrypto -lssl

.PHONY: install
install: $(SYSTEM_BIN)/$(BIN)
	$(Q) mkdir -p $(INSTALL_PATH)
	$(Q) cp $(BIN) $(INSTALL_PATH)
	$(Q) mkdir -p $(SYSROOT_DBG)/usr/lib
	$(Q) cp $(SDKSRC_DIR)/application_dexatek/dexatek/main_application/libdexatek.so $(SYSROOT_DBG)/usr/lib/
	
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
	$(Q)rm -f $(INSTALL_PATH)/$(BIN)

.PHONY: clean
clean:
	$(Q)rm -f $(root)/$(BIN)
	$(Q)find $(root) -name "*.[ado]" -exec rm -f {} \;

# general directory independent targets
%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(addprefix -D,$(DEFS)) $< -c -o $@ $(addprefix -I,$(INCS)) $(addprefix -L,$(LIBS)) 

# Autodependencies
-include $(DEPS) 