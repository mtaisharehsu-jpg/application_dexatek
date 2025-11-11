MODBUS_PATH := $(realpath $(CURDIR))/managers/modbus_manager
include $(SDKSRC_DIR)/application_dexatek/internal.mk

# define
# MODBUS_DEFINE :=

# source
MODBUS_SRCS := 
MODBUS_SRCS += $(wildcard $(MODBUS_PATH)/*.c)

# include
MODBUS_INCS := 
MODBUS_INCS += $(MODBUS_PATH)
MODBUS_INCS += $(BUILDROOT_HOST_INC)/modbus/

# library
MODBUS_LIBS :=
MODBUS_LIBS += -lmodbus

MODBUS_LIBS_PATH :=
MODBUS_LIBS_PATH += $(BUILDROOT_TARGET_LIB)

# CFLAGS
MODBUS_CFLAGS :=
MODBUS_CFLAGS += -Wl,-rpath-link=$(BUILDROOT_TARGET_LIB)
MODBUS_CFLAGS += -Wl,-rpath=/usr/lib

# LDFLAGS
MODBUS_LDFLAGS :=
MODBUS_LDFLAGS += $(addprefix -L, $(MODBUS_LIBS_PATH)) $(MODBUS_LIBS)