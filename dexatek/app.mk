app-$(CONFIG_DEXATEK_APPLICATION) += dexatek
app-$(CONFIG_DEXATEK_APPLICATION) += dexatek-lib

PHONY += dexatek dexatek-clean dexatek-install dexatek-uninstall dexatek-lib dexatek-lib-clean dexatek-lib-install
dexatek:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) all

dexatek-clean:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) clean
	
dexatek-distclean: dexatek-clean

dexatek-install:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) install

dexatek-uninstall:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) uninstall

dexatek-lib:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) lib

dexatek-lib-clean:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) lib-clean

dexatek-lib-install:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) lib-install

dexatek-lib-distclean: dexatek-lib-clean

dexatek-lib-uninstall:
	$(Q)$(MAKE) -C $(DEXATEK_MAIN_PATH) lib-uninstall