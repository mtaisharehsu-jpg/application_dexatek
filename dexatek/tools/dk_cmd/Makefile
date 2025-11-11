SDKSRC_DIR ?= $(realpath $(CURDIR)/../../../../)
include $(SDKSRC_DIR)/application_dexatek/internal.mk

# specify build flags
MAKEFLAGS += -j4

# specify build tools
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

# specify CFLAGS
LIB_CFLAGS :=

CFLAGS += -g -std=gnu99 -MMD -MP -lm -lpthread
CFLAGS += -DCONFIG_PLATFORM_LINUX=1 -DREMOVE_AGTX_BOOL
CFLAGS += $(LIB_CFLAGS)

# paths
root = $(realpath $(CURDIR))
INCS := $(root)/../../../
LIBS :=
SRCS :=
SYSTEM_BIN := $(root)/bin
INSTALL_PATH := $(SYSROOT_DBG)/usr/bin

# specify include directories
INCS += $(root)/include

# specify libary directories

# LDFLAGS
LDFLAGS :=
LDFLAGS += -Wl,--copy-dt-needed-entries

# specify source files
SRCS += $(wildcard $(root)/*.c)

# [DON'T TOUCH] calculate corresponding object files and auto-dependencies
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# specify bin
BIN = dk_cmd

# sepcify targets

.PHONY: default
default: all

.PHONY: all
all: $(root)/$(BIN)

$(root)/$(BIN): $(OBJS)
	$(Q)$(CC) $(CFLAGS) $^ -o $@ $(addprefix -L,$(LIBS)) $(LDFLAGS)

.PHONY: install
install:
	$(Q) mkdir -p $(INSTALL_PATH)
	$(Q) cp $(BIN) $(INSTALL_PATH)

$(SYSTEM_BIN)/$(BIN): $(root)/$(BIN) | $(SYSTEM_BIN)
	$(Q)install -m 777 $< $(dir $@)

$(SYSTEM_BIN) :
	$(Q)mkdir -p $@
	

.PHONY: uninstall
uninstall:
	$(Q)rm -rf $(SYSTEM_BIN)
#$(Q)rm -rf $(SYSTEM_BIN) $(INSTALL_PATH)/$(LIB_REALNAME)
	$(Q)if [ -d $(SYSTEMFS)/bin ]; then \
		rmdir --ignore-fail-on-non-empty $(SYSTEMFS)/bin; \
	fi

.PHONY: clean
clean:
	$(Q)rm -f $(root)/$(BIN)
	$(Q)find $(root) -name "*.[ado]" -exec rm -f {} \;

# general directory independent targets
%.o: %.c
	$(Q)$(CC) $(CFLAGS) $(addprefix -D,$(DEFS)) $< -c -o $@ $(addprefix -I,$(INCS)) $(addprefix -L,$(LIBS))

# Autodependencies
-include $(DEPS)
