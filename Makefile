# ==============================================================================
# Makefile for rpi-doom-touch (Raspberry Pi 4B Touch & Keypad Doom Port)
# ==============================================================================

CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -std=c99
INCLUDES = -Isrc

# Detect architecture and configure Cortex-A72 optimizations on ARM/Raspberry Pi
UNAME_M := $(shell uname -m 2>/dev/null || echo unknown)
ifneq (,$(filter arm% aarch64%,$(UNAME_M)))
    ARM_FLAGS = -march=armv8-a+crc -mtune=cortex-a72
    ifeq ($(UNAME_M),armv7l)
        ARM_FLAGS += -mfpu=neon-fp-armv8 -mfloat-abi=hard
    endif
    CFLAGS += $(ARM_FLAGS)
endif

# SDL2 dependencies (pkg-config with graceful fallback)
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null || echo "")
SDL2_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2")

LDFLAGS += -lpthread -lm

# Source files
CORE_SRCS = src/hardware/config_manager.c \
            src/hardware/touch_overlay.c \
            src/hardware/matrix_keypad.c \
            src/engine/doom_bridge.c

CORE_OBJS = $(CORE_SRCS:.c=.o)
MAIN_SRC  = src/main.c
MAIN_OBJ  = src/main.o

TARGET    = doom-rpi-touch
TOOL      = tools/test_gpio_matrix

# Test targets
TEST_PROGS = tests/test_config_manager \
             tests/test_touch_math \
             tests/test_touch_render \
             tests/test_matrix_keypad \
             tests/test_event_bridge

.PHONY: all test clean tools help

all: $(TARGET) $(TOOL)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL2_CFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CORE_OBJS) $(MAIN_OBJ) $(LDFLAGS) $(SDL2_LIBS) -o $@

$(TOOL): tools/test_gpio_matrix.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) tools/test_gpio_matrix.c $(CORE_OBJS) $(LDFLAGS) -o $@

tools: $(TOOL)

# ==============================================================================
# Test Suite
# ==============================================================================
tests/test_config_manager: tests/test_config_manager.c src/hardware/config_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

tests/test_touch_math: tests/test_touch_math.c src/hardware/touch_overlay.c src/hardware/config_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -lm -o $@

tests/test_touch_render: tests/test_touch_render.c src/hardware/touch_overlay.c src/hardware/config_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -lm -o $@

tests/test_matrix_keypad: tests/test_matrix_keypad.c src/hardware/matrix_keypad.c src/hardware/config_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -lpthread -o $@

tests/test_event_bridge: tests/test_event_bridge.c src/engine/doom_bridge.c src/hardware/touch_overlay.c src/hardware/matrix_keypad.c src/hardware/config_manager.c
	$(CC) $(CFLAGS) $(INCLUDES) $^ -lm -lpthread -o $@

test: $(TEST_PROGS)
	@echo "================================================="
	@echo " Running full test suite for rpi-doom-touch...   "
	@echo "================================================="
	@./tests/test_config_manager
	@./tests/test_touch_math
	@./tests/test_touch_render
	@./tests/test_matrix_keypad
	@./tests/test_event_bridge
	@echo "================================================="
	@echo " ALL 5 TEST SUITES PASSED SUCCESSFULLY (100%)    "
	@echo "================================================="

clean:
	rm -f $(CORE_OBJS) $(MAIN_OBJ) $(TARGET) $(TOOL) $(TARGET).exe $(TOOL).exe
	rm -f $(TEST_PROGS) tests/*.exe tests/*.o
	rm -rf tmp_config_test/

help:
	@echo "rpi-doom-touch build targets:"
	@echo "  make        - Build Doom executable and GPIO keypad tool"
	@echo "  make test   - Compile and execute all 5 unit test suites"
	@echo "  make tools  - Build standalone hardware testing tools"
	@echo "  make clean  - Remove all compiled binaries and test artifacts"
