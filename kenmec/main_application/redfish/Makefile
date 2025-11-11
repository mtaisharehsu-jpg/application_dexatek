# Redfish Server Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -DCONFIG_REDFISH_ACCOUNT_DB_PATH=\"./redfish_accounts.db\" -DCONFIG_REDFISH_TOKEN_VERIFY_ENABLE=1
DEBUG_FLAGS = -g -DDEBUG
GDB_FLAGS = -O0 -g3 -ggdb3 -fno-omit-frame-pointer -DDEBUG
INCLUDES = -I./include -I../../../ -I../../.. -I../..

# Check for mbedTLS installation
MBEDTLS_CFLAGS = $(shell pkg-config --cflags mbedtls 2>/dev/null)
MBEDTLS_LIBS = $(shell pkg-config --libs mbedtls 2>/dev/null)

# Fallback if pkg-config doesn't work
ifeq ($(MBEDTLS_LIBS),)
    MBEDTLS_LIBS = -lmbedtls -lmbedx509 -lmbedcrypto
endif

# Check for libmodbus installation
MODBUS_CFLAGS = $(shell pkg-config --cflags libmodbus 2>/dev/null)
MODBUS_LIBS = $(shell pkg-config --libs libmodbus 2>/dev/null)

# Fallback if pkg-config doesn't work
ifeq ($(MODBUS_LIBS),)
    MODBUS_LIBS = -lmodbus
endif

# Libraries
LIBS = $(MBEDTLS_LIBS) $(MODBUS_LIBS) -lpthread -ldns_sd -lsqlite3 -lssl -lcrypto -lm

# Add mbedTLS includes if found
ifneq ($(MBEDTLS_CFLAGS),)
    INCLUDES += $(MBEDTLS_CFLAGS)
endif

# Add libmodbus includes if found
ifneq ($(MODBUS_CFLAGS),)
    INCLUDES += $(MODBUS_CFLAGS)
endif

# Source directory
SRC_DIR = src
INC_DIR = include

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
# Include dummy sources under src/dummy
DUMMY_SOURCES = $(wildcard $(SRC_DIR)/dummy/*.c)
SOURCES += $(DUMMY_SOURCES)
OBJECTS = $(SOURCES:.c=.o)

# Main target
TARGET = redfish_server
MAIN_SOURCE = main.c
STUBS_SOURCE = stubs.c

# Default rule
all: check-deps $(TARGET)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@test -f /usr/include/mbedtls/ssl.h || (echo "ERROR: mbedTLS headers not found. Please run 'make install-deps' first." && exit 1)
	@test -f /usr/include/modbus.h || pkg-config --exists libmodbus || (echo "ERROR: libmodbus headers not found. Please run 'make install-deps' first." && exit 1)
	@echo "Dependencies OK."

# Build target
$(TARGET): $(OBJECTS) $(MAIN_SOURCE) $(STUBS_SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(MAIN_SOURCE) $(STUBS_SOURCE) $(OBJECTS) -o $(TARGET) $(LIBS)

# Rule for object files in src directory
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Build with gdb-friendly flags and launch gdb
gdb: CFLAGS := $(filter-out -O%,$(CFLAGS)) $(GDB_FLAGS)
gdb: clean $(TARGET)
	@echo "Launching gdb with args: $(ARGS)"
	gdb -q --args ./$(TARGET) $(ARGS)

# Clean
clean:
	rm -f $(OBJECTS) $(TARGET) $(SRC_DIR)/*.d

# Clean all generated files including .d and .o files
clean-all:
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.d $(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	@echo "Installing dependencies..."
	sudo apt-get update
	sudo apt-get install -y libmbedtls-dev libavahi-compat-libdnssd-dev build-essential pkg-config libsqlite3-dev libssl-dev libmodbus-dev
	@echo "Dependencies installed. You can now run 'make' to build."

# Show variables (for debugging makefile)
show-vars:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LIBS: $(LIBS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"

# Help
help:
	@echo "Available targets:"
	@echo "  all         - Build the redfish server (default)"
	@echo "  debug       - Build with debug flags"
	@echo "  gdb         - Rebuild with -O0/-ggdb3 and launch gdb. Use ARGS=..."
	@echo "  clean       - Remove object files and executable"
	@echo "  clean-all   - Remove all generated files"
	@echo "  install-deps- Install required dependencies"
	@echo "  show-vars   - Show makefile variables"
	@echo "  help        - Show this help message"

.PHONY: all debug gdb clean clean-all install-deps show-vars help check-deps
