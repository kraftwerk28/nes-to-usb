DEVICE = stm32f103c8t6
OPENCM3_DIR = libopencm3
BUILD_DIR = build

FREERTOS_DIR = FreeRTOS-Kernel
FREERTOS_CHIP = ARM_CM3

ENTRY = nes_to_usb

TPIU_FREQ ?= 72000000
FIFO_NAME ?= itm.fifo
OPENOCD_CFG = target/stm32f1x.cfg
OPENOCD_GDB_PORT = 3333

SOURCES += src/$(ENTRY).c src/utils.c

SOURCES += $(wildcard $(FREERTOS_DIR)/*.c)
SOURCES += $(FREERTOS_DIR)/portable/GCC/$(FREERTOS_CHIP)/port.c
SOURCES += $(FREERTOS_DIR)/portable/MemMang/heap_3.c

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
BIN_ELF = $(BUILD_DIR)/$(ENTRY).elf
BIN_BIN = $(BUILD_DIR)/$(ENTRY).bin
BIN_HEX = $(BUILD_DIR)/$(ENTRY).hex

ENABLE_ITM ?= 0
LOG_LEVEL ?= 30

CFLAGS += -Os -ggdb3 -DENABLE_ITM=$(ENABLE_ITM) -DLOG_LEVEL=$(LOG_LEVEL)
CFLAGS += -Wall -Wno-unused-variable -Wno-unused-function
LDFLAGS += -static -nostartfiles
LDLIBS += -Wl,--start-group -lc -lnosys -Wl,--end-group
LDLIBS += -Wl,--gc-sections

# FreeRTOS-specific defines
CFLAGS += -DGCC_$(FREERTOS_CHIP)
CFLAGS += -Iinclude -I$(FREERTOS_DIR)/include -I$(FREERTOS_DIR)/portable/GCC/$(FREERTOS_CHIP)

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

.PHONY: clean all flash bear fmt gdb openocd

all: $(BIN_BIN) $(BIN_ELF)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@printf "  CC      $<\n"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCH_FLAGS) -o $@ -c $<

clean:
	$(Q)$(RM) -rfv $(BUILD_DIR) generated.*.ld

flash: $(BIN_ELF)
	$(Q)openocd \
		-f interface/stlink.cfg \
		-f $(OPENOCD_CFG) \
		-c "program $< verify reset exit"

openocd: $(FIFO_NAME) check-fifo
	$(Q)openocd \
		-f interface/stlink.cfg \
		-f $(OPENOCD_CFG) \

erase:
	$(Q)st-flash erase

gdb: $(BIN_ELF)
	@arm-none-eabi-gdb -x .gdbinit -ex "target extended-remote :$(OPENOCD_GDB_PORT)" $<

bear:
	@mkdir -p $(BUILD_DIR)
	bear -- make -j -B

fmt:
	@clang-format -i {src,include}/**

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
