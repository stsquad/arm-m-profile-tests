
PREFIX=/usr/bin
GCC=$(firstword $(wildcard $(PREFIX)/arm-*eabi*-gcc))
OBJCOPY=$(firstword $(wildcard $(PREFIX)/arm*objcopy))

ifeq ($(GCC),)
$(error missing GCC)
endif

DEBUG=-g

CFLAGS=-mcpu=cortex-m3 -mthumb -ffreestanding -nostdlib -nostartfiles -nodefaultlibs $(DEBUG) -Wall -Wextra -Werror -Wl,--build-id=none
LDFLAGS=-static

CFLAGS+=-Os

all: test1-kern.bin
all: test3-kern.bin
all: test4-kern.bin
all: test5-kern.bin
all: test6-kern.bin
all: test7-kern.bin
all: test8-kern.bin
all: test9-kern.bin
all: test10-kern.bin
all: test11-kern.bin
all: test12-kern.bin
all: test13-kern.bin

test1-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o testme.o test1.o
test3-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o test3.o
test4-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o testme.o test4.o
test5-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o testme.o test5.o
test6-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o test6.o
test7-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o test7.o
test8-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m.o test8.o inst_skip.o
test9-kern.elf: cortexm.ld common.ld setup.o armv7m.o init-m-test9.o testme.o test9.o
test10-kern.elf:cortexm.ld common.ld setup.o armv7m.o init-m.o testme.o test10.o
test11-kern.elf:cortexm.ld common.ld setup.o armv7m.o init-m.o test11-buserr.o inst_skip.o
test12-kern.elf:cortexm.ld common.ld setup.o test12.o
test13-kern.elf:cortexm.ld common.ld setup.o armv7m.o init-m.o test13-undef.o inst_skip.o testme.o

clean:
	rm -f *.o *.elf *.map *.bin *.img

%.o: %.c armv7m.h
	$(GCC) -c $< -o $@ $(CFLAGS)

%.o: %.S
	$(GCC) -c $< -o $@ $(CFLAGS)

%.elf:
	$(GCC) -T$(firstword $(filter %.ld,$^)) -Wl,-Map=$*.map -o $@ $(CFLAGS) $($*_CFLAGS) $(LDFLAGS) $(*_LDFLAGS) $(filter %.o,$^) -lgcc

%.bin: %.elf
	$(OBJCOPY) -S -O binary $< $@

%.img: %.bin
	rm -f $@
	dd if=/dev/zero of=$@ bs=1M count=64 conv=sparse
	dd if=$< of=$@ bs=1M conv=notrunc,sparse
