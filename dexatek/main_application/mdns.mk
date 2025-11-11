MDNS_PATH := $(realpath $(CURDIR))/services/mdns_service
include $(SDKSRC_DIR)/application_dexatek/internal.mk

# defines (if any)
# MDNS_DEFS :=

# sources
MDNS_SRCS :=
MDNS_SRCS += $(wildcard $(MDNS_PATH)/*.c)

# include paths
MDNS_INCS :=
MDNS_INCS += $(MDNS_PATH)
MDNS_INCS += $(APP_DEXATEK_PATH)/library/mDNSResponder/include

# libraries
MDNS_LIBS :=
MDNS_LIBS += -ldns_sd

MDNS_LIBS_PATH :=
# mDNSResponder built outputs and sysroot debug lib dir
MDNS_LIBS_PATH += $(APP_DEXATEK_PATH)/library/mDNSResponder/lib
MDNS_LIBS_PATH += $(SYSROOT_DBG)/usr/lib
# fallback to buildroot target lib if used
MDNS_LIBS_PATH += $(BUILDROOT_TARGET_LIB)

# CFLAGS
MDNS_CFLAGS :=
MDNS_CFLAGS += -Wl,-rpath-link=$(SYSROOT_DBG)/usr/lib
MDNS_CFLAGS += -Wl,-rpath=/usr/lib

# LDFLAGS
MDNS_LDFLAGS :=
MDNS_LDFLAGS += $(addprefix -L, $(MDNS_LIBS_PATH)) $(MDNS_LIBS)


