SDKSRC_DIR?=$(realpath $(CURDIR)/../../..)
include $(SDKSRC_DIR)/application_dexatek/internal.mk

CC=$(CROSS_COMPILE)gcc

INC := -I. -I./../include -I$(APP_INC)
CFLAGS := -mcpu=$(CONFIG_TARGET_CPU) $(INC) -g -Wall -O2 ${PERF_NO_OMIT_FP} -MMD
PIC_CFLAGS := $(CFLAGS) -fPIC

TARGETS = libgpio.a libgpio_pic.a

SRCS=$(shell find -name '*.c')
OBJS=$(SRCS:.c=.o)
PIC_OBJS=$(SRCS:.c=.pic.o)

.PHONY: default
default: all

.PHONY: all
all: $(TARGETS)

libgpio.a: $(OBJS)
	$(Q)$(AR) rcs $@ $^

libgpio_pic.a: $(PIC_OBJS)
	$(Q)$(AR) rcs $@ $^

%.o: %.c
	$(Q)$(CC) $< -c $(CFLAGS) -o $@

%.pic.o: %.c
	$(Q)$(CC) $< -c $(PIC_CFLAGS) -o $@

.PHONY : install
install:
	$(Q)mkdir -p $(APP_LIB)
	$(Q)cp -f $(TARGETS) $(APP_LIB)

.PHONY : uninstall
uninstall:
	$(Q)rm -f $(APP_LIB)/$(TARGETS)

.PHONY: clean
clean:
	$(Q)find . -type f -name "*.[do]" -exec rm -f {} \;
	$(Q)find . -type f -name "*.pic.o" -exec rm -f {} \;
	$(Q)rm -rf $(TARGETS)

.PHONY: distclean
distclean: uninstall clean
