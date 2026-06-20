# ============================================================
# Canny Edge Detection on RISC-V
# Dual-target Makefile: host (x86) and RISC-V cross-compilation
# ============================================================
# --- Compilers ---
HOST_CXX   = g++
RV_CXX     = riscv64-unknown-elf-g++
# --- Flags ---
HOST_FLAGS = -std=c++17 -Wall -Wextra -O2
RV_FLAGS   = -std=c++17 -Wall -Wextra -march=rv64gcv -mabi=lp64d -O2
RV_FLAGS  += -static
# --- Directories ---
SRC_DIR    = src
TEST_DIR   = tests
RVV_DIR    = rvv
BUILD_DIR  = build
# --- Sources (exclude generator AND the embedded-image entry point --
#     riscv_main.cpp needs embedded_image.h and has its own main(), so it
#     must never be compiled together with main.cpp) ---
SRCS       = $(filter-out $(SRC_DIR)/Generate_test_image.cpp $(SRC_DIR)/riscv_main.cpp, $(wildcard $(SRC_DIR)/*.cpp))
# --- QEMU ---
QEMU       = qemu-riscv64
VLEN       = 128
QEMU_FLAGS = -cpu rv64,v=true,vlen=$(VLEN)
# --- Targets ---
HOST_BIN   = $(BUILD_DIR)/canny
RV_BIN     = $(BUILD_DIR)/canny_rv
# --- GoogleTest ---
GTEST_INC  = /usr/local/include
GTEST_LIB  = /usr/local/lib
# ============================================================
# Embedded image (works around no fopen() under QEMU on bare-metal RISC-V)
# riscv_main.cpp #includes "embedded_image.h" -- generated here. It lands
# in include/, so no extra -I flags are needed beyond what's already below.
# Change EMBED_RAW/EMBED_W/EMBED_H to embed a different test image.
# ============================================================
EMBED_RAW    = test_136x136.raw  
EMBED_W      = 136
EMBED_H      = 136
EMBED_NAME   = EMBEDDED_IMAGE
EMBED_HDR    = include/embedded_image.h
EMBED_SRCS   = $(filter-out $(SRC_DIR)/main.cpp $(SRC_DIR)/riscv_main.cpp $(SRC_DIR)/Generate_test_image.cpp, $(wildcard $(SRC_DIR)/*.cpp))
RV_EMBED_BIN = $(BUILD_DIR)/canny_rv_embedded
DECODE_DIR   = $(BUILD_DIR)/decoded
QEMU_LOG     = $(BUILD_DIR)/qemu_output.log
# ============================================================
RV_RVV_SRCS = $(filter-out \
    $(SRC_DIR)/main.cpp \
    $(SRC_DIR)/riscv_main.cpp \
    $(SRC_DIR)/Generate_test_image.cpp, \
    $(wildcard $(SRC_DIR)/*.cpp))

RV_RVV_BIN = $(BUILD_DIR)/canny_rv_vectorized

RVV_QEMU_LOG   = $(BUILD_DIR)/qemu_output_rvv.log
RVV_DECODE_DIR = $(BUILD_DIR)/decoded_rvv

.PHONY: all host canny_rv run clean test dirs embed-header canny_rv_embedded run-embedded run-embedded-decode
# Create build directory
dirs:
	mkdir -p $(BUILD_DIR)
# Host build (x86) for local testing
host: dirs
	@echo "Compiling for host (x86)..."
	$(HOST_CXX) $(HOST_FLAGS) -I include $(SRCS) -o $(HOST_BIN)
	@echo "Done: $(HOST_BIN)"
# Cross-compile for RISC-V
canny_rv: dirs
	@echo "Cross-compiling for RISC-V (rv64gcv, VLEN=$(VLEN))..."
	$(RV_CXX) $(RV_FLAGS) -I include $(SRCS) -o $(RV_BIN)
	@echo "Done: $(RV_BIN)"
# Run on QEMU
run: canny_rv
	$(QEMU) $(QEMU_FLAGS) $(RV_BIN)
# Regenerate embedded_image.h from EMBED_RAW.
embed-header:
	@echo "Embedding $(EMBED_RAW) ($(EMBED_W)x$(EMBED_H)) into $(EMBED_HDR)..."
	python3 scripts/raw_to_header.py $(EMBED_RAW) $(EMBED_HDR) $(EMBED_W) $(EMBED_H) $(EMBED_NAME)

# Cross-compile the embedded-image entry point (riscv_main.cpp). Depends on
# embed-header so embedded_image.h always reflects EMBED_RAW/W/H first.
canny_rv_embedded: dirs embed-header
	@echo "Cross-compiling embedded-image build for RISC-V..."
	$(RV_CXX) $(RV_FLAGS) -I include $(EMBED_SRCS) -o $(RV_EMBED_BIN)
	@echo "Done: $(RV_EMBED_BIN)"

# Full pipeline with RVV Gaussian + RVV magnitude
canny_rv_vectorized: dirs embed-header
	@echo "Cross-compiling vectorized pipeline..."
	$(RV_CXX) $(RV_FLAGS) -I include $(RV_RVV_SRCS) \
		-o $(RV_RVV_BIN)
	@echo "Done: $(RV_RVV_BIN)"

# Plain run -- prints everything, including the hex dumps, to your terminal.
run-embedded: canny_rv_embedded
	$(QEMU) $(QEMU_FLAGS) $(RV_EMBED_BIN)

# Same run, but captures stdout and decodes the hex dumps back into real
# .raw files under build/decoded/ -- since save_image() can't actually
# write files on this target, this is how you get viewable output (open
# them with your existing visualize.py).
run-embedded-decode: canny_rv_embedded
	$(QEMU) $(QEMU_FLAGS) $(RV_EMBED_BIN) > $(QEMU_LOG)
	python3 scripts/decoder_dump.py $(QEMU_LOG) $(DECODE_DIR)
	@echo "Decoded .raw files are in $(DECODE_DIR)/"

run-vectorized: canny_rv_vectorized
	$(QEMU) $(QEMU_FLAGS) $(RV_RVV_BIN)

run-vectorized-decode: canny_rv_vectorized
	mkdir -p $(RVV_DECODE_DIR)
	$(QEMU) $(QEMU_FLAGS) $(RV_RVV_BIN) > $(RVV_QEMU_LOG)
	python3 scripts/decoder_dump.py $(RVV_QEMU_LOG) $(RVV_DECODE_DIR)
	@echo "Decoded .raw files are in $(RVV_DECODE_DIR)/"
# Host-side GoogleTest
test: dirs
	@echo "--- Running Gaussian tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_gaussian.cpp src/gaussian.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_gaussian
	./$(BUILD_DIR)/test_gaussian
	@echo "--- Running Sobel tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_sobel_gtest.cpp src/sobel.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_sobel
	./$(BUILD_DIR)/test_sobel
	@echo "--- Running Magnitude tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_magnitude.cpp src/magnitude.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_magnitude
	./$(BUILD_DIR)/test_magnitude
	@echo "--- Running Direction tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_direction.cpp src/direction.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_direction
	./$(BUILD_DIR)/test_direction
	@echo "--- Running NMS tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_nms.cpp src/nms.cpp src/sobel.cpp src/gaussian.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_nms
	./$(BUILD_DIR)/test_nms
	@echo "--- Running Double Threshold tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_double_threshold.cpp src/double_threshold.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_double_threshold
	./$(BUILD_DIR)/test_double_threshold
	@echo "--- Running Hysteresis tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_hysteresis.cpp src/hysteresis.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_hysteresis
	./$(BUILD_DIR)/test_hysteresis

# RVV Gaussian correctness test
TEST_GAUSS_VEC_BIN = $(BUILD_DIR)/test_gaussian_vectorized_rv

test_gaussian_vectorized: dirs
	@echo "--- Building vectorized Gaussian test ---"
	$(RV_CXX) $(RV_FLAGS) -I include \
		tests/test_gaussian_vectorized.cpp \
		src/gaussian.cpp \
		src/gaussian_vectorized.cpp \
		-o $(TEST_GAUSS_VEC_BIN)

	@echo "--- Running under QEMU ---"
	$(QEMU) $(QEMU_FLAGS) $(TEST_GAUSS_VEC_BIN)

# RVV Magnitude correctness test
TEST_MAG_VEC_BIN = $(BUILD_DIR)/test_magnitude_vectorized_rv

test_magnitude_vectorized: dirs
	@echo "--- Building vectorized magnitude test ---"
	$(RV_CXX) $(RV_FLAGS) -I include \
		tests/test_magnitude_vectorized.cpp \
		src/magnitude.cpp \
		src/magnitude_vectorized.cpp \
		-o $(TEST_MAG_VEC_BIN)

	@echo "--- Running under QEMU ---"
	$(QEMU) $(QEMU_FLAGS) $(TEST_MAG_VEC_BIN)


# Clean
clean:
	rm -rf $(BUILD_DIR)
