app-$(CONFIG_KENMEC_APPLICATION) += kenmec

PHONY += kenmec kenmec-clean kenmec-install kenmec-uninstall
kenmec:
	$(Q)$(MAKE) -C $(KENMEC_MAIN_PATH) all

kenmec-clean:
	$(Q)$(MAKE) -C $(KENMEC_MAIN_PATH) clean
	
kenmec-distclean: kenmec-clean

kenmec-install:
	$(Q)$(MAKE) -C $(KENMEC_MAIN_PATH) install

kenmec-uninstall:
	$(Q)$(MAKE) -C $(KENMEC_MAIN_PATH) uninstall