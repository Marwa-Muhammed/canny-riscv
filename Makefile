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

# --- QEMU ---
QEMU       = qemu-riscv64
VLEN       = 128
QEMU_FLAGS = -cpu rv64,v=true,vlen=$(VLEN)

# --- Targets ---
RV_BIN     = $(BUILD_DIR)/canny_rv

# --- GoogleTest ---
GTEST_INC  = /usr/local/include
GTEST_LIB  = /usr/local/lib

# ============================================================

.PHONY: all canny_rv run clean test dirs

all: dirs canny_rv

# Create build directory
dirs:
	mkdir -p $(BUILD_DIR)

# Cross-compile for RISC-V
canny_rv: dirs
	@echo "Cross-compiling for RISC-V (rv64gcv, VLEN=$(VLEN))..."
	@echo "No source files yet - add them to src/"

# Run on QEMU
run: canny_rv
	$(QEMU) $(QEMU_FLAGS) $(RV_BIN)

# Host-side GoogleTest
test: dirs
	@echo "--- Running Gaussian tests ---"
	$(HOST_CXX) $(HOST_FLAGS) -I include -I$(GTEST_INC) \
		tests/test_gaussian.cpp src/gaussian.cpp \
		-L$(GTEST_LIB) -lgtest -lgtest_main -lpthread \
		-o $(BUILD_DIR)/test_gaussian
	./$(BUILD_DIR)/test_gaussian
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

# Clean
clean:
	rm -rf $(BUILD_DIR)
