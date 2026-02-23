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

# --- Optional cJSON vendor/submodule ---
CJSON_DIR ?= external/cjson
CJSON_SRC := $(CJSON_DIR)/cJSON.c
CJSON_INC := $(CJSON_DIR)

# --- AnonyMaestro (mbedTLS) ---
ANONY_DIR ?= external/anonymaestro
ANONY_INC := $(ANONY_DIR)/include
ANONY_CFG_HEADER ?= mbedtls_config.h

ANONY_BUILD_DIR := $(BUILD_DIR)/anonymaestro

MBEDTLS_LIBS := \
  $(ANONY_BUILD_DIR)/library/libmbedtls.a \
  $(ANONY_BUILD_DIR)/library/libmbedx509.a \
  $(ANONY_BUILD_DIR)/library/libmbedcrypto.a

# --- Flags ---
CSTD      ?= -std=c11
OPTFLAGS  ?= -O2
WARNFLAGS := -Wall -Wextra -Wpedantic

CPPFLAGS += -I$(MOD_INC_DIR) -I$(UTL_INC_DIR)
CPPFLAGS += -I$(ANONY_INC)

CFLAGS   += $(CSTD) $(WARNFLAGS) $(OPTFLAGS) -MMD -MP
CFLAGS   += -DMBEDTLS_USER_CONFIG_FILE=\"maestromodules/tls_config.h\"

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

DEPS := $(MOD_OBJS:.o=.d) $(UTL_OBJS:.o=.d)

ifeq ($(WITH_CJSON),1)
  DEPS += $(CJSON_OBJ:.o=.d)
endif

# --- Libraries ---
LIB_CORE := $(LIB_DIR)/libmaestrocore.a

# --- Manual test ---
MANUAL_TEST_SRC := manual_http_simple_filelog.c
MANUAL_TEST_BIN := $(BUILD_DIR)/http_manual_test

# =============================
# ========= Targets ===========
# =============================

.PHONY: all core clean make-test anonymaestro

all: core

core: anonymaestro $(LIB_CORE)

# --- Directory creation ---
$(LIB_DIR) $(OBJ_DIR) $(OBJ_DIR)/modules $(OBJ_DIR)/utils $(OBJ_DIR)/external:
	@mkdir -p $@

# --- Build mbedTLS via CMake ---
anonymaestro: $(MBEDTLS_LIBS)

$(MBEDTLS_LIBS):
	@mkdir -p $(ANONY_BUILD_DIR)
	cmake -S $(ANONY_DIR) -B $(ANONY_BUILD_DIR) \
		-DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF
	cmake --build $(ANONY_BUILD_DIR)

# --- Compile rules ---
$(OBJ_DIR)/modules/%.o: $(MOD_SRC_DIR)/%.c | $(OBJ_DIR)/modules
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(UTL_SRC_DIR)/%.c | $(OBJ_DIR)/utils
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/external/cjson.o: $(CJSON_SRC) | $(OBJ_DIR)/external
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --- Archive build ---
$(LIB_CORE): anonymaestro $(LIB_DIR) $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	@rm -f $@
	$(AR) rcs $@ $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(RANLIB) $@

# --- Manual test build ---
make-test: core $(MANUAL_TEST_BIN)
	@echo "--------------------------------------"
	@echo "   BUILT MANUAL HTTP TEST             "
	@echo "--------------------------------------"

$(MANUAL_TEST_BIN): $(MANUAL_TEST_SRC) $(LIB_CORE) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< \
		-L$(LIB_DIR) -lmaestrocore \
		$(MBEDTLS_LIBS) \
		-lpthread -lm \
		-o $@

# --- Housekeeping ---
clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)

