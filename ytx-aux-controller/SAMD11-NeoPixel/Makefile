# Makefile for YTX Aux Controller (SAMD11D14AM)
# 
# Prerequisites: arm-none-eabi-gcc toolchain
# Install on macOS: brew install arm-none-eabi-gcc
#

# Project name
PROJECT = SAMD11-NeoPixel

# Toolchain
# Use ARM official toolchain if available, otherwise try system PATH
#
TOOLCHAIN_PATH ?= $(HOME)/arm-gnu-toolchain/bin
ifneq ($(wildcard $(TOOLCHAIN_PATH)/arm-none-eabi-gcc),)
    CC = $(TOOLCHAIN_PATH)/arm-none-eabi-gcc
    OBJCOPY = $(TOOLCHAIN_PATH)/arm-none-eabi-objcopy
    OBJDUMP = $(TOOLCHAIN_PATH)/arm-none-eabi-objdump
    SIZE = $(TOOLCHAIN_PATH)/arm-none-eabi-size
else
    CC = arm-none-eabi-gcc
    OBJCOPY = arm-none-eabi-objcopy
    OBJDUMP = arm-none-eabi-objdump
    SIZE = arm-none-eabi-size
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
ASF_DIR = $(SRC_DIR)/ASF

# Target MCU
MCU = cortex-m0plus

# Linker script
LDSCRIPT = $(ASF_DIR)/sam0/utils/linker_scripts/samd11/gcc/samd11d14am_flash.ld

# Defines
DEFINES = \
	-DNDEBUG \
	-D__SAMD11D14AM__ \
	-DBOARD=SAMD11_XPLAINED_PRO \
	-DARM_MATH_CM0PLUS=true \
	-DSPI_CALLBACK_MODE=false \
	-DUSART_CALLBACK_MODE=true \
	-DTC_ASYNC=true \
	-DSYSTICK_MODE

# Include paths
INCLUDES = \
	-I$(SRC_DIR) \
	-I$(SRC_DIR)/config \
	-I$(SRC_DIR)/NeoPixel \
	-I$(ASF_DIR)/common/boards \
	-I$(ASF_DIR)/common/utils \
	-I$(ASF_DIR)/common2/services/delay \
	-I$(ASF_DIR)/common2/services/delay/sam0 \
	-I$(ASF_DIR)/sam0/boards \
	-I$(ASF_DIR)/sam0/boards/samd11_xplained_pro \
	-I$(ASF_DIR)/sam0/drivers/port \
	-I$(ASF_DIR)/sam0/drivers/sercom \
	-I$(ASF_DIR)/sam0/drivers/sercom/spi \
	-I$(ASF_DIR)/sam0/drivers/sercom/usart \
	-I$(ASF_DIR)/sam0/drivers/system \
	-I$(ASF_DIR)/sam0/drivers/system/clock \
	-I$(ASF_DIR)/sam0/drivers/system/clock/clock_samd09_d10_d11 \
	-I$(ASF_DIR)/sam0/drivers/system/interrupt \
	-I$(ASF_DIR)/sam0/drivers/system/interrupt/system_interrupt_samd10_d11 \
	-I$(ASF_DIR)/sam0/drivers/system/pinmux \
	-I$(ASF_DIR)/sam0/drivers/system/power \
	-I$(ASF_DIR)/sam0/drivers/system/power/power_sam_d_r_h \
	-I$(ASF_DIR)/sam0/drivers/system/reset \
	-I$(ASF_DIR)/sam0/drivers/system/reset/reset_sam_d_r_h \
	-I$(ASF_DIR)/sam0/drivers/tc \
	-I$(ASF_DIR)/sam0/drivers/tc/tc_sam_d_r_h \
	-I$(ASF_DIR)/sam0/utils \
	-I$(ASF_DIR)/sam0/utils/cmsis/samd11/include \
	-I$(ASF_DIR)/sam0/utils/cmsis/samd11/source \
	-I$(ASF_DIR)/sam0/utils/header_files \
	-I$(ASF_DIR)/sam0/utils/preprocessor \
	-I$(ASF_DIR)/thirdparty/CMSIS/Include \
	-I$(ASF_DIR)/thirdparty/CMSIS/Lib/GCC

# Compiler flags
CFLAGS = \
	-mcpu=$(MCU) \
	-mthumb \
	-Os \
	-ffunction-sections \
	-fdata-sections \
	-fno-strict-aliasing \
	-fcommon \
	-Wall \
	-Wno-strict-prototypes \
	-Wno-missing-prototypes \
	-Wpointer-arith \
	-std=gnu99 \
	$(DEFINES) \
	$(INCLUDES)

# Linker flags
LDFLAGS = \
	-mcpu=$(MCU) \
	-mthumb \
	-Wl,--gc-sections \
	-Wl,--entry=Reset_Handler \
	-Wl,-Map=$(BUILD_DIR)/$(PROJECT).map \
	-T$(LDSCRIPT) \
	--specs=nano.specs \
	-L$(ASF_DIR)/thirdparty/CMSIS/Lib/GCC \
	-larm_cortexM0l_math \
	-lm

# Source files
SOURCES = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/setup.c \
	$(SRC_DIR)/feedback.c \
	$(SRC_DIR)/interrupts.c \
	$(SRC_DIR)/main-controller-comms.c \
	$(SRC_DIR)/utils.c \
	$(SRC_DIR)/variables.c \
	$(SRC_DIR)/NeoPixel/NeoPixels.c \
	$(ASF_DIR)/common/utils/interrupt/interrupt_sam_nvic.c \
	$(ASF_DIR)/common2/services/delay/sam0/systick_counter.c \
	$(ASF_DIR)/sam0/boards/samd11_xplained_pro/board_init.c \
	$(ASF_DIR)/sam0/drivers/port/port.c \
	$(ASF_DIR)/sam0/drivers/sercom/sercom.c \
	$(ASF_DIR)/sam0/drivers/sercom/sercom_interrupt.c \
	$(ASF_DIR)/sam0/drivers/sercom/spi/spi.c \
	$(ASF_DIR)/sam0/drivers/sercom/usart/usart.c \
	$(ASF_DIR)/sam0/drivers/sercom/usart/usart_interrupt.c \
	$(ASF_DIR)/sam0/drivers/system/clock/clock_samd09_d10_d11/clock.c \
	$(ASF_DIR)/sam0/drivers/system/clock/clock_samd09_d10_d11/gclk.c \
	$(ASF_DIR)/sam0/drivers/system/interrupt/system_interrupt.c \
	$(ASF_DIR)/sam0/drivers/system/pinmux/pinmux.c \
	$(ASF_DIR)/sam0/drivers/system/system.c \
	$(ASF_DIR)/sam0/drivers/tc/tc_interrupt.c \
	$(ASF_DIR)/sam0/drivers/tc/tc_sam_d_r_h/tc.c \
	$(ASF_DIR)/sam0/utils/cmsis/samd11/source/gcc/startup_samd11.c \
	$(ASF_DIR)/sam0/utils/cmsis/samd11/source/system_samd11.c \
	$(ASF_DIR)/sam0/utils/syscalls/gcc/syscalls.c

# Object files (in build directory, preserving structure)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Default target
all: $(BUILD_DIR)/$(PROJECT).bin $(BUILD_DIR)/$(PROJECT).hex
	@echo ""
	@echo "Build complete!"
	@$(SIZE) $(BUILD_DIR)/$(PROJECT).elf

# Create build directory structure
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Create binary
$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

# Create hex
$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

# Create listing
$(BUILD_DIR)/$(PROJECT).lss: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -h -S $< > $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install to binaries folder
install: $(BUILD_DIR)/$(PROJECT).bin
	cp $(BUILD_DIR)/$(PROJECT).bin ../../binaries/aux_new.bin
	@echo "Copied to binaries/aux_new.bin"

# Show size
size: $(BUILD_DIR)/$(PROJECT).elf
	$(SIZE) --format=berkeley $<

# Help
help:
	@echo "YTX Aux Controller Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build project (default)"
	@echo "  clean    - Remove build directory"
	@echo "  install  - Copy binary to binaries/aux_new.bin"
	@echo "  size     - Show memory usage"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Prerequisites:"
	@echo "  brew install arm-none-eabi-gcc"

.PHONY: all clean install size help

