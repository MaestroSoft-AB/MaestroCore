# =============================
# ===== MaestroCore Makefile ===
# =============================

UNAME_S := $(shell uname -s)

CC      ?= cc
AR      ?= ar
RANLIB  ?= ranlib

BUILD_DIR := build
OBJ_DIR    := $(BUILD_DIR)/obj
LIB_DIR    := $(BUILD_DIR)/lib

# --- Project layout ---
MOD_SRC_DIR := modules/src
UTL_SRC_DIR := utils/src
MOD_INC_DIR := modules/include
UTL_INC_DIR := utils/include

# --- Unity / CMock Configuration (Recursive structure) ---
TEST_DIR     := test
CMOCK_DIR    := $(TEST_DIR)/cmock
# Unity is now inside CMock after a recursive clone
UNITY_DIR    := $(CMOCK_DIR)/vendor/unity
TEST_OBJ_DIR := $(OBJ_DIR)/tests
TEST_BIN     := $(BUILD_DIR)/test_suite

CMOCK_SRC    := $(CMOCK_DIR)/src/cmock.c
CMOCK_INC    := $(CMOCK_DIR)/src
CMOCK_LIB    := $(CMOCK_DIR)/lib/cmock.rb
MOCK_OUT_DIR := $(TEST_DIR)/mocks

# --- Optional cJSON vendor/submodule ---
CJSON_DIR ?= external/cjson
CJSON_SRC := $(CJSON_DIR)/cJSON.c
CJSON_INC := $(CJSON_DIR)

# --- Flags ---
CSTD      ?= -std=c11
OPTFLAGS  ?= -O2
WARNFLAGS := -Wall -Wextra -Wpedantic

CPPFLAGS += -I$(MOD_INC_DIR) -I$(UTL_INC_DIR)
CFLAGS   += $(CSTD) $(WARNFLAGS) $(OPTFLAGS) -MMD -MP

# Test-specific flags
TEST_CPPFLAGS := $(CPPFLAGS) -I$(UNITY_DIR)/src -I$(CMOCK_INC) -I$(MOCK_OUT_DIR) -I$(MOD_INC_DIR)/maestromodules -DCMOCK

# Enable JSON: make JSON=1
ifeq ($(JSON),1)
  CPPFLAGS += -I$(CJSON_INC)
  CFLAGS   += -DMAESTROUTILS_WITH_CJSON
  WITH_CJSON := 1
else
  WITH_CJSON := 0
endif

# --- Sources ---
MOD_SRCS := $(wildcard $(MOD_SRC_DIR)/*.c)
UTL_SRCS_ALL := $(wildcard $(UTL_SRC_DIR)/*.c)

ifeq ($(WITH_CJSON),1)
  UTL_SRCS := $(UTL_SRCS_ALL)
else
  UTL_SRCS := $(filter-out $(UTL_SRC_DIR)/json_utils.c,$(UTL_SRCS_ALL))
endif

ifeq ($(WITH_CJSON),1)
  CJSON_OBJ := $(OBJ_DIR)/external/cjson.o
else
  CJSON_OBJ :=
endif

# --- Objects ---
MOD_OBJS := $(patsubst $(MOD_SRC_DIR)/%.c,$(OBJ_DIR)/modules/%.o,$(MOD_SRCS))
UTL_OBJS := $(patsubst $(UTL_SRC_DIR)/%.c,$(OBJ_DIR)/utils/%.o,$(UTL_SRCS))

# --- Mock Files ---
MOCK_HEADERS := $(MOD_INC_DIR)/maestromodules/tcp_client.h
MOCKS_SRCS   := $(MOCK_OUT_DIR)/Mocktcp_client.c
MOCKS_OBJS   := $(OBJ_DIR)/tests/Mocktcp_client.o


# --- Dependency files ---
DEPS := $(MOD_OBJS:.o=.d) $(UTL_OBJS:.o=.d)
ifeq ($(WITH_CJSON),1)
  DEPS += $(CJSON_OBJ:.o=.d)
endif

# --- Libraries ---
LIB_CORE    := $(LIB_DIR)/libmaestrocore.a

# =============================
# ========= Targets ===========
# =============================

.PHONY: all core clean test install uninstall print

all: core

core: $(LIB_CORE)

# --- Test Target ---
test: $(TEST_BIN)
	@echo "--------------------------------------"
	@echo "   RUNNING UNIT TESTS (UNITY + CMOCK) "
	@echo "--------------------------------------"
	@./$(TEST_BIN)

# --- Directory creation ---
$(LIB_DIR) $(OBJ_DIR) $(OBJ_DIR)/modules $(OBJ_DIR)/utils $(OBJ_DIR)/external $(TEST_OBJ_DIR) $(MOCK_OUT_DIR):
	@mkdir -p $@

# --- Archive build ---
$(LIB_CORE): $(LIB_DIR) $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(AR) rcs $@ $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(RANLIB) $@

# --- Compile rules ---
$(OBJ_DIR)/modules/%.o: $(MOD_SRC_DIR)/%.c | $(OBJ_DIR)/modules
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(UTL_SRC_DIR)/%.c | $(OBJ_DIR)/utils
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/external/cjson.o: $(CJSON_SRC) | $(OBJ_DIR)/external
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --- Test Build Rules ---

# 1. Compile Unity (src is in vendor/unity/src)
$(TEST_OBJ_DIR)/unity.o: $(UNITY_DIR)/src/unity.c | $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) -I$(UNITY_DIR)/src -c $< -o $@

# 2. Compile CMock core
$(TEST_OBJ_DIR)/cmock.o: $(CMOCK_SRC) | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 3. Generate Mock files
$(MOCK_OUT_DIR)/Mock%.c: $(MOD_INC_DIR)/maestromodules/%.h | $(MOCK_OUT_DIR)
	ruby $(CMOCK_LIB) -o$(TEST_DIR)/cmock_config.yaml $<

# 4. Compile Generated Mocks
$(OBJ_DIR)/tests/Mock%.o: $(MOCK_OUT_DIR)/Mock%.c | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 5. Compile Test Runner (Depends on Mocks being generated)
$(TEST_OBJ_DIR)/test_http.o: $(TEST_DIR)/test_http.c $(MOCKS_SRCS) | $(TEST_OBJ_DIR)
	$(CC) $(TEST_CPPFLAGS) $(CFLAGS) -c $< -o $@

# 6. Link Test Binary
$(TEST_BIN): $(TEST_OBJ_DIR)/unity.o $(TEST_OBJ_DIR)/cmock.o $(MOCKS_OBJS) $(TEST_OBJ_DIR)/test_http.o $(LIB_CORE)
	$(CC) $(CFLAGS) $^ -o $@

# --- Housekeeping ---
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(MOCK_OUT_DIR)

print:
	@echo "MOD_SRCS=$(MOD_SRCS)"
	@echo "LIB_CORE=$(LIB_CORE)"
	@echo "UNITY_DIR=$(UNITY_DIR)"

# =============================
# ========= Install ===========
# =============================

PREFIX ?= /usr/local
INCDIR := $(PREFIX)/include
LIBINSTDIR := $(PREFIX)/lib

install: all
	@mkdir -p "$(DESTDIR)$(INCDIR)" "$(DESTDIR)$(LIBINSTDIR)"
	@cp -f $(MOD_INC_DIR)/maestromodules.h "$(DESTDIR)$(INCDIR)/"
	@cp -f $(UTL_INC_DIR)/maestroutils.h    "$(DESTDIR)$(INCDIR)/"
	@mkdir -p "$(DESTDIR)$(INCDIR)/maestromodules" "$(DESTDIR)$(INCDIR)/maestroutils"
	@cp -f $(MOD_INC_DIR)/maestromodules/*.h "$(DESTDIR)$(INCDIR)/maestromodules/"
	@cp -f $(UTL_INC_DIR)/maestroutils/*.h   "$(DESTDIR)$(INCDIR)/maestroutils/"
	@cp -f $(LIB_CORE) "$(DESTDIR)$(LIBINSTDIR)/"
	@echo "Installed to $(DESTDIR)$(PREFIX)"

-include $(DEPS)
