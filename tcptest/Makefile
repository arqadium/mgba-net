#!/usr/bin/env make
# -*- coding: utf-8 -*-

# name of project. used in output binary naming
PROJECT := tcptest

# system utilities
AS    := arm-none-eabi-as
CC    := arm-none-eabi-gcc
LD    := arm-none-eabi-gcc
OCPY  := arm-none-eabi-objcopy
FIX   := gbafix
PY    := env python3
FMT   := clang-format

# flags
ASFLAGS := -mcpu=arm7tdmi -march=armv4t -mthumb-interwork -mthumb
CFLAGS  := -ansi -O2 -Wall -fno-strict-aliasing \
	-mcpu=arm7tdmi -march=armv4t -mthumb-interwork -ffreestanding
ICFLAGS := $(CFLAGS) -marm -mlong-calls
RCFLAGS := $(CFLAGS) -mthumb
LDFLAGS := -specs=$(DEVKITARM)/arm-none-eabi/lib/gba.specs

# sources
SFILES :=
CFILES := \
	main.c
HFILES :=
IMGFILES :=
PCMFILES :=
OFILES := $(SFILES:.s=.s.o) $(CFILES:.c=.c.o) \
	$(IMGFILES:.png=.png.o) $(PCMFILES:.pcm=.pcm.o)

# space-separated path list for #includes
INCLUDES :=

# space-separated library name list for linking
LIBS    := tonc
LIBDIRS := $(DEVKITPRO)/libtonc/lib

# Variable transformations for command invocation
LIB := $(patsubst %,-L%,$(LIBDIRS)) $(patsubst %,-l%,$(LIBS))
INCLUDE := $(patsubst %,-I%,$(INCLUDES))

# targets
.PHONY: default clean format

default: $(PROJECT).gba

$(IMGFILES): ;
$(PCMFILES): ;

$(PROJECT).gba: $(PROJECT).elf
	$(OCPY) -O binary $< $@
	$(FIX) $@ -p -t'TCPTEST' -cCTTE -m01 -r00 -d0

$(PROJECT).elf: $(OFILES)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIB)

%.pcm.o: %.pcm.s
	$(AS) $(ASFLAGS) -o $@ $<

%.pcm.s: %.pcm
	$(BIN2ASM) $< $@ -s `$(DIR2SYM) $<`_pcm

%.png.o: %.png
	$(GFX2O) $< $@

%.iwram.c.o: %.iwram.c
	$(CC) $(ICFLAGS) $(INCLUDE) -c -o $@ $<

%.c.o: %.c
	$(CC) $(RCFLAGS) $(INCLUDE) -c -o $@ $<

%.s.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	@echo 'Cleaning project...'
	@$(RM) $(PROJECT).gba
	@$(RM) $(PROJECT).elf
	@$(RM) $(OFILES)

format: $(CFILES) $(HFILES)
	for _file in $^; do \
		$(FMT) -i -style=file $$_file ; \
	done
	unset _file
