SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../..)

include $(SDKSRC_DIR)/application_dexatek/internal.mk
include $(APP_DEXATEK_PATH)/common.mk

# specify bin
BIN_NAME := wpa_supplicant wpa_cli wpa_passphrase

# bin paths
VAR_BIN_DIR := $(SYSROOT_DBG)/usr/sbin
BIN_TARGET := $(addprefix $(VAR_BIN_DIR)/,$(BIN_NAME))


.DEFAULT_GOAL := default

.PHONY: default
default: all

.PHONY: all
all:
	$(Q)$(MKDIR_P) $(BIN_OUT_PATH)
# $(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant clean
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant all CC="$(CC)" LD="$(LD)" \
		EXTRA_CFLAGS="-I$(LIBNL_INC) -I$(OPENSSL_INC)" \
		EXTRALIBS="-L$(LIBNL_LIB) -L$(OPENSSL_LIB)"
	$(Q)$(STRIP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_supplicant
	$(Q)$(STRIP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_cli
	$(Q)$(STRIP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_passphrase
	$(Q)$(CP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_supplicant $(BIN_OUT_PATH)
	$(Q)$(CP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_cli $(BIN_OUT_PATH)
	$(Q)$(CP) $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant/wpa_passphrase $(BIN_OUT_PATH)


# Delete all files created by building the app.
.PHONY: clean
clean:
	$(Q)$(RM) -r $(BIN_OUT_PATH)
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/wpa_supplicant clean
	

# Delete all files created by configuring, installing or building.
.PHONY: distclean
distclean: uninstall clean

.PHONY: install
install: all install-bin

.PHONY: install-bin
install-bin:
	$(Q)$(CP) -r $(BIN_OUT_PATH)/* $(VAR_BIN_DIR)

.PHONY: uninstall
uninstall: uninstall-bin 

.PHONY: uninstall-bin
uninstall-bin:
	$(Q)$(RM) $(BIN_TARGET)


-include $(DEPS)
