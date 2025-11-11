HID_PATH := $(realpath $(CURDIR))/../../dexatek/main_application/managers/hid_manager
include $(SDKSRC_DIR)/application_dexatek/internal.mk

# define
# MODBUS_DEFINE :=

# source
HID_SRCS := 
HID_SRCS += $(wildcard $(HID_PATH)/*.c)

# include
HID_INCS := 
HID_INCS += $(HID_PATH)
HID_INCS += $(BUILDROOT_HOST_INC)/libusb-1.0/

# library
HID_LIBS :=
HID_LIBS += -lusb-1.0

HID_LIBS_PATH :=
HID_LIBS_PATH += $(BUILDROOT_TARGET_LIB)

# CFLAGS
HID_CFLAGS :=
HID_CFLAGS += -Wl,-rpath-link=$(BUILDROOT_TARGET_LIB)
HID_CFLAGS += -Wl,-rpath=/usr/lib

# LDFLAGS
HID_LDFLAGS :=
HID_LDFLAGS += $(addprefix -L, $(HID_LIBS_PATH)) $(HID_LIBS)