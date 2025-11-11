PHONY :=

root := $(APP_DEXATEK_PATH)

subdir := $(root)/library
-include $(subdir)/app.mk

subdir := $(root)/dexatek
-include $(subdir)/app.mk

subdir := $(root)/kenmec
-include $(subdir)/app.mk

# Collect all conditionally enabled applications
APP_TARGETS := $(app-y)
APP_CLEAN_TARGETS := $(addsuffix -clean,$(app-y))
APP_INSTALL_TARGETS := $(addsuffix -install,$(app-y))
APP_UNINSTALL_TARGETS := $(addsuffix -uninstall,$(app-y))

PHONY += app-check
app-check:
ifeq ("$(wildcard $(KCONFIG_CONFIG))","")
	$(error Application config not found)
endif

PHONY += app library app-clean app-distclean app-install app-uninstall

app: library $(APP_TARGETS)

app-clean: library-clean $(APP_CLEAN_TARGETS)

app-distclean: app-clean
	$(Q)rm -f $(KCONFIG_CONFIG)
	$(Q)rm -f $(KCONFIG_CONFIG).old
	@echo 'Application config `$(notdir $(KCONFIG_CONFIG))` has been removed.'

app-install: library-install $(APP_INSTALL_TARGETS)

app-uninstall: library-uninstall $(APP_UNINSTALL_TARGETS)


.PHONY: $(PHONY)
