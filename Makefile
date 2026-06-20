# ============================================================
# Canny Edge Detection on RISC-V
# Dual-target Makefile: host (x86) and RISC-V cross-compilation
# ============================================================
# --- Compilers ---
HOST_CXX   = g++
RV_CXX     = riscv64-unknown-elf-g++
# --- Flags ---
HOST_FLAGS = -std=c++17 -Wall -Wextra -O0
RV_FLAGS   = -std=c++17 -Wall -Wextra -march=rv64gcv -mabi=lp64d -O0
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
VLEN       = 512
QEMU_FLAGS = -cpu rv64,v=false,vlen=$(VLEN)
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
EMBED_RAW    = test_100x75.raw  
EMBED_W      = 100
EMBED_H      = 75
EMBED_NAME   = EMBEDDED_IMAGE
EMBED_HDR    = include/embedded_image.h
EMBED_SRCS   = $(filter-out $(SRC_DIR)/main.cpp $(SRC_DIR)/Generate_test_image.cpp, $(wildcard $(SRC_DIR)/*.cpp))
RV_EMBED_BIN = $(BUILD_DIR)/canny_rv_embedded
DECODE_DIR   = $(BUILD_DIR)/decoded
QEMU_LOG     = $(BUILD_DIR)/qemu_output.log
# ============================================================
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

# Host-side GoogleTest
test: dirs
	@echo "--- Running Gaussian tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_gaussian.cpp src/gaussian.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_gaussian
	./$(BUILD_DIR)/test_gaussian
	@echo "--- Running Sobel tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_sobel.cpp src/sobel.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_sobel
	./$(BUILD_DIR)/test_sobel
	@echo "--- Running Magnitude tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_magnitude.cpp src/magnitude.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_magnitude
	./$(BUILD_DIR)/test_magnitude
	@echo "--- Running Direction tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_direction.cpp src/direction.cpp src/sobel.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_direction
	./$(BUILD_DIR)/test_direction
	@echo "--- Running NMS tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_nms.cpp src/nms.cpp src/sobel.cpp src/gaussian.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_nms
	./$(BUILD_DIR)/test_nms
	@echo "--- Running Double Threshold tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_double_threshold.cpp src/double_threshold.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_double_threshold
	./$(BUILD_DIR)/test_double_threshold
	@echo "--- Running Hysteresis tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/gtest_hysteresis.cpp src/hysteresis.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_hysteresis
	./$(BUILD_DIR)/test_hysteresis

# Clean
clean:
	rm -rf $(BUILD_DIR)
