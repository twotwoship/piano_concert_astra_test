# GNU Arm GCC, same target and floating-point ABI as ex_code.
TOOL_DIR ?= C:/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi
CC = "$(TOOL_DIR)/bin/arm-none-eabi-gcc"
OBJCOPY = "$(TOOL_DIR)/bin/arm-none-eabi-objcopy"
SIZE = "$(TOOL_DIR)/bin/arm-none-eabi-size"
ARCH = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS = $(ARCH) -DSTM32F411xE -std=gnu99 -Os -Wall -Wextra -Werror -g -ffreestanding -fno-common -I.
LDFLAGS = $(ARCH) -nostdlib -Wl,-Map=concert.map -T rom_0x08000000.lds
SOURCES = main.c buzzer.c player.c clock.c system_stm32f4xx.c generated/score_data.c
OBJECTS = crt0.o $(SOURCES:.c=.o)

all: concert.bin concert.hex

concert.elf: $(OBJECTS) rom_0x08000000.lds
	$(CC) $(OBJECTS) $(LDFLAGS) -lgcc -o $@
	$(SIZE) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

crt0.o: crt0.s
	$(CC) $(ARCH) -c $< -o $@

$(SOURCES:.c=.o): option.h device_driver.h buzzer.h player.h generated/score_data.h

concert.bin: concert.elf
	$(OBJCOPY) -O binary $< $@

concert.hex: concert.elf
	$(OBJCOPY) -O ihex $< $@

run: concert.elf
	STM32_Programmer_CLI.exe -c port=SWD -w concert.elf -v -rst

tone-test: tone_test.bin

tone_test.elf: crt0.o tone_test.o clock.o system_stm32f4xx.o rom_0x08000000.lds
	$(CC) crt0.o tone_test.o clock.o system_stm32f4xx.o $(ARCH) -nostdlib -Wl,-Map=tone_test.map -T rom_0x08000000.lds -lgcc -o $@
	$(SIZE) $@

tone_test.o: option.h device_driver.h

tone_test.bin: tone_test.elf
	$(OBJCOPY) -O binary $< $@

clean:
	powershell.exe -NoProfile -Command "$$ErrorActionPreference = 'Stop'; foreach ($$file in @('crt0.o','main.o','buzzer.o','player.o','clock.o','system_stm32f4xx.o','generated/score_data.o','concert.elf','concert.bin','concert.hex','concert.map','tone_test.o','tone_test.elf','tone_test.bin','tone_test.map')) { if (Test-Path -LiteralPath $$file -PathType Leaf) { Remove-Item -LiteralPath $$file -Force } }; Write-Output 'Firmware build outputs cleaned.'"

# Keep clean and compilation sequential, even with make -j.
rebuild:
	$(MAKE) clean
	$(MAKE) all

.PHONY: all run clean rebuild tone-test
