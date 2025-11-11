mod := $(notdir $(subdir))

app-$(CONFIG_LIBGPIO) += libgpio
PHONY += libgpio libgpio-clean libgpio-distclean
PHONY += libgpio-install libgpio-uninstall
libgpio:
	$(Q)$(MAKE) -C $(LIBGPIO_PATH) all

libgpio-clean:
	$(Q)$(MAKE) -C $(LIBGPIO_PATH) clean

libgpio-distclean:
	$(Q)$(MAKE) -C $(LIBGPIO_PATH) distclean

libgpio-install:
	$(Q)$(MAKE) -C $(LIBGPIO_PATH) install

libgpio-uninstall:
	$(Q)$(MAKE) -C $(LIBGPIO_PATH) uninstall

app-$(CONFIG_WPA_SUPPLICANT_RTW) += wpa_supplicant_rtw
PHONY += wpa_supplicant_rtw wpa_supplicant_rtw-clean wpa_supplicant_rtw-distclean
PHONY += wpa_supplicant_rtw-install wpa_supplicant_rtw-uninstall
wpa_supplicant_rtw:
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/build all

wpa_supplicant_rtw-clean:
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/build clean

wpa_supplicant_rtw-distclean:
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/build distclean

wpa_supplicant_rtw-install:
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/build install

wpa_supplicant_rtw-uninstall:
	$(Q)$(MAKE) -C $(WPA_SUPPLICANT_RTW_PATH)/build uninstall

app-$(CONFIG_LIBMODBUS) += libmodbus
PHONY += libmodbus libmodbus-clean libmodbus-distclean
PHONY += libmodbus-install libmodbus-uninstall
libmodbus:
	$(Q)$(MAKE) -C $(LIBMODBUS_PATH)/build all

libmodbus-clean:
	$(Q)$(MAKE) -C $(LIBMODBUS_PATH)/build clean

libmodbus-distclean:
	$(Q)$(MAKE) -C $(LIBMODBUS_PATH)/build distclean

libmodbus-install:
	$(Q)$(MAKE) -C $(LIBMODBUS_PATH)/build install

libmodbus-uninstall:
	$(Q)$(MAKE) -C $(LIBMODBUS_PATH)/build uninstall

app-$(CONFIG_MDNS) += mdns
PHONY += mdns mdns-clean mdns-distclean
PHONY += mdns-install mdns-uninstall
mdns:
	$(Q)$(MAKE) -C $(MDNS_PATH)/build all

mdns-clean:
	$(Q)$(MAKE) -C $(MDNS_PATH)/build clean

mdns-distclean:
	$(Q)$(MAKE) -C $(MDNS_PATH)/build distclean

mdns-install:
	$(Q)$(MAKE) -C $(MDNS_PATH)/build install

mdns-uninstall:
	$(Q)$(MAKE) -C $(MDNS_PATH)/build uninstall

PHONY += $(mod) $(mod)-clean $(mod)-distclean
PHONY += $(mod)-install $(mod)-uninstall
$(mod): $(app-y)
$(mod)-clean: $(addsuffix -clean,$(app-y))
$(mod)-distclean: $(addsuffix -distclean,$(app-y))
$(mod)-install: $(addsuffix -install,$(app-y))
$(mod)-uninstall: $(addsuffix -uninstall,$(app-y))
	
APP_BUILD_DEPS += $(mod)
APP_CLEAN_DEPS += $(mod)-clean
APP_DISTCLEAN_DEPS += $(mod)-distclean
APP_INTALL_DEPS += $(mod)-install
APP_UNINTALL_DEPS += $(mod)-uninstall
