# ---- Layout ------------------------------------------------------------------
SRC   := src
BUILD := build
OBJ   := $(BUILD)/obj
BIN   := $(BUILD)/bin
LIB   := $(BUILD)/lib

find_src = $(sort $(shell find $(SRC)/$(1) -name '*.c' 2>/dev/null))
to_obj   = $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(1))
lib_file = lib$(1).so

NASH_NAME    := nash
INIT_NAME    := nash-init
LIBCNTR_NAME := nash-cntr

NASH_BIN    := $(BIN)/$(NASH_NAME)
INIT_BIN    := $(BIN)/$(INIT_NAME)
LIBCNTR_BIN := $(LIB)/$(call lib_file,$(LIBCNTR_NAME))

NASH_OBJ    := $(call to_obj,$(call find_src,engine)) $(call to_obj,$(call find_src,cli)) $(OBJ)/main.o
INIT_OBJ    := $(call to_obj,$(call find_src,init))
LIBCNTR_OBJ := $(call to_obj,$(call find_src,container))
UTILS_OBJ   := $(call to_obj,$(call find_src,utils))

ALL_OBJ := $(NASH_OBJ) $(INIT_OBJ) $(LIBCNTR_OBJ) $(UTILS_OBJ)

NASH_TO_LIBCNTR := $(shell realpath -m --relative-to=$(dir $(NASH_BIN)) $(dir $(LIBCNTR_BIN)))

# ---- Toolchain & flags -------------------------------------------------------
CC       := gcc
CFLAGS   ?= -O2 -g
CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?=

override CFLAGS   += -std=gnu11 -Wall -Wextra -Werror -Wmissing-prototypes
override CPPFLAGS += -I$(SRC) -MMD -MP

$(LIBCNTR_OBJ): override CFLAGS += -fPIC
$(UTILS_OBJ):   override CFLAGS += -fPIC -fvisibility=hidden

# ---- Dependencies ------------------------------------------------------------
NASH_STATIC_LIBS      := ocispec blake3
NASH_SHARED_LIBS      := uuid json-c sodium archive collectc sqlite3 smartcols
NASH_INTERNAL_LIBS    := $(LIBCNTR_NAME)

INIT_STATIC_LIBS      :=
INIT_SHARED_LIBS      :=

LIBCNTR_STATIC_LIBS   :=
LIBCNTR_SHARED_LIBS   := uuid cap

# ---- Top-level targets -------------------------------------------------------
.PHONY: all clean nash init libcntr
.DELETE_ON_ERROR:

all: nash

nash:    $(NASH_BIN)
init:    $(INIT_BIN)
libcntr: $(LIBCNTR_BIN)

# ---- Linking helpers ---------------------------------------------------------
static_libs = -Wl,-Bstatic $(foreach lib,$(1),-l$(lib)) -Wl,-Bdynamic
shared_libs = $(foreach lib,$(1),-l$(lib))

# ---- Linking rules -----------------------------------------------------------
$(LIBCNTR_BIN): $(LIBCNTR_OBJ) $(UTILS_OBJ)
	@mkdir -p $(@D)
	@$(CC) -shared -Wl,-soname,$(@F) $(LDFLAGS) $^ \
	    $(call static_libs,$(LIBCNTR_STATIC_LIBS)) \
	    $(call shared_libs,$(LIBCNTR_SHARED_LIBS)) \
	    $(LDLIBS) -o $@

$(INIT_BIN): $(INIT_OBJ) $(UTILS_OBJ)
	@mkdir -p $(@D)
	@$(CC) $(LDFLAGS) $^ \
	    $(call static_libs,$(INIT_STATIC_LIBS)) \
	    $(call shared_libs,$(INIT_SHARED_LIBS)) \
	    $(LDLIBS) -o $@

$(NASH_BIN): $(NASH_OBJ) $(UTILS_OBJ) $(LIBCNTR_BIN)
	@mkdir -p $(@D)
	@$(CC) $(LDFLAGS) $(filter %.o,$^) \
	    -L$(dir $(LIBCNTR_BIN)) \
	    $(call shared_libs,$(NASH_INTERNAL_LIBS)) \
	    $(call static_libs,$(NASH_STATIC_LIBS)) \
	    $(call shared_libs,$(NASH_SHARED_LIBS)) \
	    -Wl,-rpath,'$$ORIGIN/$(NASH_TO_LIBCNTR)' \
	    $(LDLIBS) -o $@

# ---- Compilation rules -------------------------------------------------------
$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(ALL_OBJ:.o=.d)
