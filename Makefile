# ==============================================================================
# ===== MaestroCore Makefile (3.6.x LTS Edition) ===============================
# ==============================================================================

UNAME_S := $(shell uname -s)

CC      ?= cc
AR      ?= ar
RANLIB  ?= ranlib

# --- Directories ---
BUILD_DIR := build
OBJ_DIR    := $(BUILD_DIR)/obj
LIB_DIR    := $(BUILD_DIR)/lib

# --- Project Layout ---
MOD_SRC_DIR := modules/src
UTL_SRC_DIR := utils/src
MOD_INC_DIR := modules/include
UTL_INC_DIR := utils/include

# --- mbedTLS 3.6.x Configuration ---
# LTS version has a flatter include structure compared to 4.x
MBEDTLS_DIR := external/mbedtls
MBEDTLS_INC := $(MBEDTLS_DIR)/include

# --- Unity / CMock Configuration (Recursive structure) ---
TEST_DIR     := test
CMOCK_DIR    := $(TEST_DIR)/cmock
UNITY_DIR    := $(CMOCK_DIR)/vendor/unity
TEST_OBJ_DIR := $(OBJ_DIR)/tests
TEST_BIN     := $(BUILD_DIR)/test_suite

CMOCK_SRC    := $(CMOCK_DIR)/src/cmock.c
CMOCK_INC    := $(CMOCK_DIR)/src
CMOCK_LIB    := $(CMOCK_DIR)/lib/cmock.rb
MOCK_OUT_DIR := $(TEST_DIR)/mocks

# --- Optional cJSON vendor/submodule ---
CJSON_DIR ?= ../cJSON
CJSON_SRC := $(CJSON_DIR)/cJSON.c
CJSON_INC := $(CJSON_DIR)

# --- Compiler & Preprocessor Flags ---
CSTD      ?= -std=c11
OPTFLAGS  ?= -O2
WARNFLAGS := -Wall -Wextra -Wpedantic

# Include project headers and mbedTLS headers
CPPFLAGS += -I$(MOD_INC_DIR) -I$(UTL_INC_DIR) -I$(MBEDTLS_INC)
# Use our specific user configuration file
CPPFLAGS += -DMBEDTLS_CONFIG_FILE='<maestromodules/tls_config.h>'

CFLAGS   += $(CSTD) $(WARNFLAGS) $(OPTFLAGS) -MMD -MP

# Test-specific flags
TEST_CPPFLAGS := $(CPPFLAGS) -I$(UNITY_DIR)/src -I$(CMOCK_INC) -I$(MOCK_OUT_DIR) -I$(MOD_INC_DIR)/maestromodules -DCMOCK

# mbedTLS Library linking
# In 3.6.x, libs are found in the library/ folder after running 'make lib'
MBEDTLS_LIBS := -L$(MBEDTLS_DIR)/library -lmbedtls -lmbedx509 -lmbedcrypto

# --- Build Feature: JSON Support ---
ifeq ($(JSON),1)
  CPPFLAGS += -I$(CJSON_INC)
  CFLAGS   += -DMAESTROUTILS_WITH_CJSON
  WITH_CJSON := 1
else
  WITH_CJSON := 0
endif

# --- Source Selection ---
MOD_SRCS := $(wildcard $(MOD_SRC_DIR)/*.c)
UTL_SRCS_ALL := $(wildcard $(UTL_SRC_DIR)/*.c)

ifeq ($(WITH_CJSON),1)
  UTL_SRCS := $(UTL_SRCS_ALL)
else
  UTL_SRCS := $(filter-out $(UTL_SRC_DIR)/json_utils.c,$(UTL_SRCS_ALL))
endif

# --- Object Files ---
MOD_OBJS := $(patsubst $(MOD_SRC_DIR)/%.c,$(OBJ_DIR)/modules/%.o,$(MOD_SRCS))
UTL_OBJS := $(patsubst $(UTL_SRC_DIR)/%.c,$(OBJ_DIR)/utils/%.o,$(UTL_SRCS))

ifeq ($(WITH_CJSON),1)
  CJSON_OBJ := $(OBJ_DIR)/external/cjson.o
else
  CJSON_OBJ :=
endif

# --- Mock Files & Objects ---
MOCK_HEADERS := $(MOD_INC_DIR)/maestromodules/tcp_client.h
MOCKS_SRCS   := $(MOCK_OUT_DIR)/Mocktcp_client.c
MOCKS_OBJS   := $(OBJ_DIR)/tests/Mocktcp_client.o

# --- Dependency files ---
DEPS := $(MOD_OBJS:.o=.d) $(UTL_OBJS:.o=.d)
ifeq ($(WITH_CJSON),1)
  DEPS += $(CJSON_OBJ:.o=.d)
endif

# --- Output Libraries ---
LIB_CORE := $(LIB_DIR)/libmaestrocore.a

# ==============================================================================
# ========= Build Targets ======================================================
# ==============================================================================

.PHONY: all core clean test install uninstall print mbedtls_libs

all: core

# Build target for mbedTLS 3.6.x libraries
# This version uses a standard Makefile that doesn't require Python/jsonschema
mbedtls_libs:
	@echo "Building mbedTLS 3.6.x static libraries..."
	$(MAKE) -C $(MBEDTLS_DIR) lib

core: $(LIB_CORE)

# --- Test Target ---
test: $(TEST_BIN)
	@echo "------------------------------------------------"
	@echo "    RUNNING UNIT TESTS (UNITY + CMOCK + TLS)    "
	@echo "------------------------------------------------"
	@./$(TEST_BIN)

# --- Directory Creation ---
$(LIB_DIR) $(OBJ_DIR) $(OBJ_DIR)/modules $(OBJ_DIR)/utils $(OBJ_DIR)/external $(TEST_OBJ_DIR) $(MOCK_OUT_DIR):
	@mkdir -p $@

# --- Static Library Build ---
$(LIB_CORE): $(LIB_DIR) $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(AR) rcs $@ $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(RANLIB) $@

# --- Compilation Rules ---
$(OBJ_DIR)/modules/%.o: $(MOD_SRC_DIR)/%.c | $(OBJ_DIR)/modules
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(UTL_SRC_DIR)/%.c | $(OBJ_DIR)/utils
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/external/cjson.o: $(CJSON_SRC) | $(OBJ_DIR)/external
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --- Test Build Rules ---

# 1. Compile Unity Core
$(TEST_OBJ_DIR)/unity.o: $(UNITY_DIR)/src/unity.c | $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) -I$(UNITY_DIR)/src -c $< -o $@

# 2. Compile CMock Core
$(TEST_OBJ_DIR)/cmock.o: $(CMOCK_SRC) | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 3. Generate Mocks using Ruby/CMock
$(MOCK_OUT_DIR)/Mock%.c: $(MOD_INC_DIR)/maestromodules/%.h | $(MOCK_OUT_DIR)
	ruby $(CMOCK_LIB) -o$(TEST_DIR)/cmock_config.yaml $<

# 4. Compile Generated Mocks
$(OBJ_DIR)/tests/Mock%.o: $(MOCK_OUT_DIR)/Mock%.c | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 5. Compile Main Test Suite
$(TEST_OBJ_DIR)/test_http.o: $(TEST_DIR)/test_http.c $(MOCKS_SRCS) | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 6. Link Test Binary (Linking against core lib and mbedTLS)
$(TEST_BIN): $(TEST_OBJ_DIR)/unity.o $(TEST_OBJ_DIR)/cmock.o $(MOCKS_OBJS) $(TEST_OBJ_DIR)/test_http.o $(LIB_CORE)
	$(CC) $(CFLAGS) $^ -o $@ $(MBEDTLS_LIBS)

# --- Cleanup ---
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(MOCK_OUT_DIR)

# --- Debug Printing ---
print:
	@echo "Sources: $(MOD_SRCS)"
	@echo "Includes: $(CPPFLAGS)"
	@echo "Libraries: $(MBEDTLS_LIBS)"

# ==============================================================================
# ========= Installation =======================================================
# ==============================================================================

PREFIX ?= /usr/local
INCDIR := $(PREFIX)/include
LIBINSTDIR := $(PREFIX)/lib

install: all
	@echo "Installing MaestroCore to $(DESTDIR)$(PREFIX)..."
	@mkdir -p "$(DESTDIR)$(INCDIR)" "$(DESTDIR)$(LIBINSTDIR)"
	@cp -f $(MOD_INC_DIR)/maestromodules.h "$(DESTDIR)$(INCDIR)/"
	@cp -f $(UTL_INC_DIR)/maestroutils.h    "$(DESTDIR)$(INCDIR)/"
	@mkdir -p "$(DESTDIR)$(INCDIR)/maestromodules" "$(DESTDIR)$(INCDIR)/maestroutils"
	@cp -f $(MOD_INC_DIR)/maestromodules/*.h "$(DESTDIR)$(INCDIR)/maestromodules/"
	@cp -f $(UTL_INC_DIR)/maestroutils/*.h   "$(DESTDIR)$(INCDIR)/maestroutils/"
	@cp -f $(LIB_CORE) "$(DESTDIR)$(LIBINSTDIR)/"
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling MaestroCore from $(DESTDIR)$(PREFIX)..."
	@rm -f "$(DESTDIR)$(INCDIR)/maestromodules.h"
	@rm -f "$(DESTDIR)$(INCDIR)/maestroutils.h"
	@rm -rf "$(DESTDIR)$(INCDIR)/maestromodules"
	@rm -rf "$(DESTDIR)$(INCDIR)/maestroutils"
	@rm -f "$(DESTDIR)$(LIBINSTDIR)/libmaestrocore.a"
	@echo "Uninstallation complete."

-include $(DEPS)
