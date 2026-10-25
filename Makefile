# Makefile - Haiku Radeon Legacy Driver
#
# Portabilidad: en Haiku enlaza las APIs reales del kernel; en el host de
# desarrollo compila el backend simulado. `make test` no necesita hardware.

CC      ?= cc
CXX     ?= c++
PYTHON  ?= python3

CFLAGS   ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -std=c11
CXXFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -std=c++17
CPPFLAGS += -Isrc -Isrc/common -MMD -MP

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

BUILD := build

# --- fuentes ---------------------------------------------------------
ASM_COMMON := src/asm/common/rg_asm_ref.c
ASM_ARCH_C :=
ASM_ARCH_S :=
ifeq ($(UNAME_M),arm64)
  ASM_ARCH_S += src/asm/arm/detile_neon.S
endif
ifeq ($(UNAME_M),aarch64)
  ASM_ARCH_S += src/asm/arm/detile_neon.S
endif
ifeq ($(UNAME_M),x86_64)
  ASM_ARCH_S += src/asm/x86/format_convert_sse2.S src/asm/x86/fast_blit_avx2.S
endif

SHIM_C := src/shim/radeon_shim.c src/shim/amdgpu_shim.c

SERVER_CXX := \
	src/server/main.cpp \
	src/server/memory_manager.cpp \
	src/server/gart.cpp \
	src/server/cs_verifier.cpp \
	src/server/ring_manager.cpp \
	src/server/firmware.cpp \
	src/server/ipc_server.cpp \
	src/server/asic/rg_asic.cpp \
	src/server/legacy/rg_legacy.cpp \
	src/server/legacy/uvd_decoder.cpp \
	src/server/legacy/rg_legacy_advanced.cpp \
	src/server/legacy/dual_gpu_switcher.cpp \
	src/server/legacy/hardware_resquicios.cpp \
	src/server/gcn/gpuvm.cpp \
	src/server/gcn/ip_blocks.cpp \
	src/server/radeon_hardening.cpp \
	src/server/radeon_hud.cpp \
	src/server/radeon_supervisor.cpp \
	src/server/radeon_simd_dispatch.cpp \
	src/server/r600_streamout_compute.cpp \
	src/server/ipc_zerocopy.cpp \
	src/server/rocm_runtime.cpp \
	src/server/vram_compression.cpp

LVP_CXX := src/lvp_hw/lvp_planner.cpp src/lvp_hw/lvp_exec_cpu.cpp

LIB_CXX := $(filter-out src/server/main.cpp,$(SERVER_CXX)) $(LVP_CXX)
LIB_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_CXX)) \
            $(patsubst %.c,$(BUILD)/%.o,$(SHIM_C)) \
            $(patsubst %.c,$(BUILD)/%.o,$(ASM_COMMON)) \
            $(patsubst %.S,$(BUILD)/%.o,$(ASM_ARCH_S))

TEST_BINS := $(BUILD)/rg_asm_check $(BUILD)/rg_test

.PHONY: all userland helper helper-kernel helper-asm shim test test-quick \
        test-asan asm-test lint lint-fast typecheck clean docs asic-db

all: userland $(TEST_BINS) $(BUILD)/radeon_hd.o

# --- compilacion -----------------------------------------------------
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

$(BUILD)/libradeon_gpud.a: $(LIB_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

userland: $(BUILD)/libradeon_gpud.a $(BUILD)/radeon_gpud

$(BUILD)/radeon_gpud: $(BUILD)/src/server/main.o $(BUILD)/libradeon_gpud.a
	$(CXX) $(CXXFLAGS) $< -o $@ $(BUILD)/libradeon_gpud.a

shim: $(patsubst %.c,$(BUILD)/%.o,$(SHIM_C))

# Helper: en Haiku usa Jam/árbol de Haiku; en el host valida el fallback.
helper helper-kernel: $(BUILD)/radeon_hd.o
ifeq ($(UNAME_S),Haiku)
	@echo "Haiku detectado: use el Jamfile del arbol de Haiku para el modulo real"
else
	@echo "helper: compilado en modo simulado ($(BUILD)/radeon_hd.o)"
endif

$(BUILD)/radeon_hd.o: src/helper/radeon_hd.c src/helper/helper_ioctls.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

helper-asm: helper

# --- tests -----------------------------------------------------------
$(BUILD)/rg_asm_check: $(BUILD)/src/asm/common/rg_asm_check.o $(BUILD)/src/asm/common/rg_asm_ref.o $(patsubst %.S,$(BUILD)/%.o,$(ASM_ARCH_S))
	$(CC) $^ -o $@ -lm

$(BUILD)/rg_test: tests/test_main.cpp $(BUILD)/libradeon_gpud.a
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(BUILD)/libradeon_gpud.a

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)

test: all
	@echo "== rg_asm_check (K1-K13) =="
	$(BUILD)/rg_asm_check
	@echo "== rg_test (servidor/verificador/shims) =="
	$(BUILD)/rg_test
	@echo "== radeon_gpud --selftest =="
	$(BUILD)/radeon_gpud --selftest

test-quick: $(BUILD)/rg_asm_check $(BUILD)/rg_test
	$(BUILD)/rg_asm_check >/dev/null && $(BUILD)/rg_test >/dev/null && echo "quick tests: OK"

asm-test: $(BUILD)/rg_asm_check
	$(BUILD)/rg_asm_check

test-asan: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
test-asan: CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
test-asan:
	@mkdir -p $(BUILD)/asan
	$(MAKE) BUILD=$(BUILD)/asan test

# --- calidad ---------------------------------------------------------
lint-fast:
	@set -e; for f in $(SHIM_C) $(ASM_COMMON) src/helper/radeon_hd.c; do \
		$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $$f; done
	@set -e; for f in $(SERVER_CXX) $(LVP_CXX); do \
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fsyntax-only $$f; done
	@echo "lint-fast: OK"

lint: lint-fast
	@echo "lint: sin reglas adicionales (no hay linter externo configurado)"

typecheck: lint-fast

# --- datos y docs ----------------------------------------------------
asic-db:
	$(PYTHON) scripts/gen_asic_db.py --output src/server/asic/rg_asic_db.inc

docs: asic-db
	@echo "docs: PROGRESS.md / DECISIONS.md / REGISTER_OWNERSHIP.md"
	@test -s docs/PROGRESS.md && test -s docs/DECISIONS.md && \
		test -s docs/REGISTER_OWNERSHIP.md
	@echo "docs: OK"

clean:
	rm -rf $(BUILD)
	@echo "clean: OK"
