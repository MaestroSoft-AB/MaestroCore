# =============================
# ===== MaestroCore Makefile ===
# =============================

UNAME_S := $(shell uname -s)

CC     ?= cc
AR     ?= ar
RANLIB ?= ranlib

BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
LIB_DIR   := $(BUILD_DIR)/lib

# --- Project layout ---
MOD_SRC_DIR := modules/src
UTL_SRC_DIR := utils/src

MOD_INC_DIR := modules/include
UTL_INC_DIR := utils/include

# --- Optional cJSON vendor/submodule ---
CJSON_DIR ?= external/cjson
CJSON_SRC := $(CJSON_DIR)/cJSON.c
CJSON_INC := $(CJSON_DIR)

# --- Flags ---
CSTD     ?= -std=c11
OPTFLAGS ?= -O2
WARNFLAGS := -Wall -Wextra -Wpedantic

CPPFLAGS += -I$(MOD_INC_DIR) -I$(UTL_INC_DIR)
CFLAGS   += $(CSTD) $(WARNFLAGS) $(OPTFLAGS) -MMD -MP

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

# utils: build everything EXCEPT json_utils.c unless JSON=1
UTL_SRCS_ALL := $(wildcard $(UTL_SRC_DIR)/*.c)

ifeq ($(WITH_CJSON),1)
  UTL_SRCS := $(UTL_SRCS_ALL)
else
  UTL_SRCS := $(filter-out $(UTL_SRC_DIR)/json_utils.c,$(UTL_SRCS_ALL))
endif

# Optional cJSON object
ifeq ($(WITH_CJSON),1)
  CJSON_OBJ := $(OBJ_DIR)/external/cjson.o
else
  CJSON_OBJ :=
endif

# --- Objects ---
MOD_OBJS := $(patsubst $(MOD_SRC_DIR)/%.c,$(OBJ_DIR)/modules/%.o,$(MOD_SRCS))
UTL_OBJS := $(patsubst $(UTL_SRC_DIR)/%.c,$(OBJ_DIR)/utils/%.o,$(UTL_SRCS))

# --- Dependency files ---
DEPS := $(MOD_OBJS:.o=.d) $(UTL_OBJS:.o=.d)
ifeq ($(WITH_CJSON),1)
  DEPS += $(CJSON_OBJ:.o=.d)
endif

# --- Libraries ---
LIB_MODULES := $(LIB_DIR)/libmaestromodules.a
LIB_UTILS   := $(LIB_DIR)/libmaestroutils.a
LIB_CORE    := $(LIB_DIR)/libmaestrocore.a

# =============================
# ========= Targets ===========
# =============================

.PHONY: all core modules utils clean print install uninstall

all: core

core: $(LIB_CORE)

modules: $(LIB_MODULES)

utils: $(LIB_UTILS)

$(LIB_DIR):
	@mkdir -p $@

$(OBJ_DIR):
	@mkdir -p $@

$(OBJ_DIR)/modules:
	@mkdir -p $@

$(OBJ_DIR)/utils:
	@mkdir -p $@

$(OBJ_DIR)/external:
	@mkdir -p $@

# --- Archive build ---
$(LIB_CORE): $(LIB_DIR) $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(AR) rcs $@ $(MOD_OBJS) $(UTL_OBJS) $(CJSON_OBJ)
	$(RANLIB) $@

$(LIB_MODULES): $(LIB_DIR) $(MOD_OBJS)
	$(AR) rcs $@ $(MOD_OBJS)
	$(RANLIB) $@

$(LIB_UTILS): $(LIB_DIR) $(UTL_OBJS) $(CJSON_OBJ)
	$(AR) rcs $@ $(UTL_OBJS) $(CJSON_OBJ)
	$(RANLIB) $@

# --- Compile rules ---
$(OBJ_DIR)/modules/%.o: $(MOD_SRC_DIR)/%.c | $(OBJ_DIR) $(OBJ_DIR)/modules
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/utils/%.o: $(UTL_SRC_DIR)/%.c | $(OBJ_DIR) $(OBJ_DIR)/utils
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/external/cjson.o: $(CJSON_SRC) | $(OBJ_DIR) $(OBJ_DIR)/external
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --- Housekeeping ---
clean:
	rm -rf $(BUILD_DIR)

print:
	@echo "WITH_CJSON=$(WITH_CJSON)"
	@echo "MOD_SRCS=$(MOD_SRCS)"
	@echo "UTL_SRCS=$(UTL_SRCS)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LIB_CORE=$(LIB_CORE)"

# =============================
# ========= Install ===========
# =============================

# Install to PREFIX (default /usr/local):
PREFIX ?= /usr/local
INCDIR := $(PREFIX)/include
LIBINSTDIR := $(PREFIX)/lib

# Installs:
#  - umbrella headers: maestromodules.h, maestroutils.h
#  - namespaced headers: maestromodules/*, maestroutils/*
#  - the libs you built (core/modules/utils)
install: all
	@mkdir -p "$(DESTDIR)$(INCDIR)" "$(DESTDIR)$(LIBINSTDIR)"
	@# umbrella headers
	@cp -f $(MOD_INC_DIR)/maestromodules.h "$(DESTDIR)$(INCDIR)/"
	@cp -f $(UTL_INC_DIR)/maestroutils.h   "$(DESTDIR)$(INCDIR)/"
	@# namespaced dirs
	@mkdir -p "$(DESTDIR)$(INCDIR)/maestromodules" "$(DESTDIR)$(INCDIR)/maestroutils"
	@cp -f $(MOD_INC_DIR)/maestromodules/*.h "$(DESTDIR)$(INCDIR)/maestromodules/"
	@cp -f $(UTL_INC_DIR)/maestroutils/*.h   "$(DESTDIR)$(INCDIR)/maestroutils/"
	@# libs
	@cp -f $(LIB_CORE) "$(DESTDIR)$(LIBINSTDIR)/"
	@echo "Installed to $(DESTDIR)$(PREFIX)"

uninstall:
	@rm -f "$(DESTDIR)$(INCDIR)/maestromodules.h" "$(DESTDIR)$(INCDIR)/maestroutils.h"
	@rm -rf "$(DESTDIR)$(INCDIR)/maestromodules" "$(DESTDIR)$(INCDIR)/maestroutils"
	@rm -f "$(DESTDIR)$(LIBINSTDIR)/libmaestrocore.a"
	@rm -f "$(DESTDIR)$(LIBINSTDIR)/libmaestromodules.a"
	@rm -f "$(DESTDIR)$(LIBINSTDIR)/libmaestroutils.a"
	@echo "Uninstalled from $(DESTDIR)$(PREFIX)"

-include $(DEPS)

