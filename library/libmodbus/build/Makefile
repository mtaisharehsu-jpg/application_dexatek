SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../..)

include $(SDKSRC_DIR)/application_dexatek/internal.mk
include $(APP_DEXATEK_PATH)/common.mk

# build target folder
OTHSA_BUILD_FOLDER := libmodbus/build
LIBMODBUS_OUT_PATH := $(LIBMODBUS_PATH)/src/.libs

# specify so
SO_NAME := libmodbus.so
SO_NAME_VER := libmodbus.so.5
VAR_SO_DIR := $(SYSROOT_DBG)/usr/lib
SO_TARGET := $(addprefix $(VAR_SO_DIR)/,$(SO_NAME))
SO_TARGET_VER := $(addprefix $(VAR_SO_DIR)/,$(SO_NAME_VER))

.DEFAULT_GOAL := default

.PHONY: default
default: all

.PHONY: all
all:
	$(Q)$(MKDIR_P) $(LIB_OUT_PATH)
	$(Q)cd $(LIBMODBUS_PATH) && ./configure --host=arm-linux-gnueabihf \
								--prefix=$(LIBMODBUS_PATH) \
								--libdir=$(BUILDROOT_OUTPUT_PATH)/target/usr/lib \
								--includedir=$(BUILDROOT_HOST_INC) 
	$(Q)cd $(LIBMODBUS_PATH) && make
	$(Q)cd $(LIBMODBUS_PATH) && make install

# Delete all files created by building the app.
.PHONY: clean
clean:
	$(Q)$(RM) -r $(LIBMODBUS_OUT_PATH)
# $(Q)cd $(LIBMODBUS_PATH) && make clean

# Delete all files created by configuring, installing or building.
.PHONY: distclean
distclean: uninstall clean

.PHONY: install
install: all install-bin

.PHONY: install-bin
install-bin:
	$(Q)$(CP) $(LIBMODBUS_OUT_PATH)/$(SO_NAME) $(VAR_SO_DIR)
	$(Q)cd $(VAR_SO_DIR) && ln -sf $(SO_NAME) $(SO_NAME_VER)

.PHONY: uninstall
uninstall: uninstall-bin

.PHONY: uninstall-bin
uninstall-bin:
	$(Q)$(RM) $(SO_TARGET)
	$(Q)$(RM) $(SO_TARGET_VER)

-include $(DEPS)
