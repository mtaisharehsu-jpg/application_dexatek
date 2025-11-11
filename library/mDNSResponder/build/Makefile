SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../..)

include $(SDKSRC_DIR)/application_dexatek/internal.mk
include $(APP_DEXATEK_PATH)/common.mk

# specify bin
BIN_NAME := mdnsd 
VAR_BIN_DIR := $(SYSROOT_DBG)/sbin
BIN_TARGET := $(addprefix $(VAR_BIN_DIR)/,$(BIN_NAME))

# specify so
SO_NAME := libdns_sd.so
VAR_SO_DIR := $(SYSROOT_DBG)/usr/lib
SO_TARGET := $(addprefix $(VAR_SO_DIR)/,$(SO_NAME))


.DEFAULT_GOAL := default

.PHONY: default
default: all

.PHONY: all
all:
	$(Q)$(MKDIR_P) $(BIN_OUT_PATH)
	$(Q)$(MKDIR_P) $(LIB_OUT_PATH)
	$(Q)$(MKDIR_P) $(INC_PATH)
	$(Q)$(MAKE) -C $(MDNS_PATH)/mDNSPosix CC="$(CC)" LD="$(LD) -shared" STRIP="$(STRIP)" os=linux
	$(Q)$(STRIP) $(MDNS_PATH)/mDNSPosix/build/prod/mdnsd
	$(Q)$(STRIP) $(MDNS_PATH)/mDNSPosix/build/prod/libdns_sd.so
	$(Q)$(CP) $(MDNS_PATH)/mDNSPosix/build/prod/mdnsd $(BIN_OUT_PATH)
	$(Q)$(CP) $(MDNS_PATH)/mDNSPosix/build/prod/libdns_sd.so $(LIB_OUT_PATH)
	$(Q)$(CP) $(MDNS_PATH)/mDNSShared/dns_sd.h $(INC_PATH)

# Delete all files created by building the app.
.PHONY: clean
clean:
	$(Q)$(RM) -r $(BIN_OUT_PATH)
	$(Q)$(RM) -r $(LIB_OUT_PATH)
	$(Q)$(MAKE) -C $(MDNS_PATH)/mDNSPosix os=linux clean
	

# Delete all files created by configuring, installing or building.
.PHONY: distclean
distclean: uninstall clean

.PHONY: install
install: all install-bin

.PHONY: install-bin
install-bin:
	$(Q)$(CP) $(BIN_OUT_PATH)/mdnsd $(VAR_BIN_DIR)
	$(Q)$(CP) $(LIB_OUT_PATH)/libdns_sd.so $(VAR_SO_DIR)

.PHONY: uninstall
uninstall: uninstall-bin

.PHONY: uninstall-bin
uninstall-bin:
	$(Q)$(RM) $(BIN_TARGET)
	$(Q)$(RM) $(SO_TARGET)

-include $(DEPS)
