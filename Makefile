.RECIPEPREFIX = >
DEVKITXENON ?= /usr/local/xenon
CC = $(DEVKITXENON)/bin/xenon-gcc
OBJCOPY = $(DEVKITXENON)/bin/xenon-objcopy
CFLAGS = -DXENON -m32 -mno-altivec -fno-pic -mpowerpc64 -mhard-float -I$(DEVKITXENON)/usr/include
LDFLAGS = -n -T $(DEVKITXENON)/app.lds -L$(DEVKITXENON)/xenon/lib/32 -L$(DEVKITXENON)/usr/lib

all: default.elf32

default.elf: main.c
> $(CC) $(CFLAGS) main.c $(LDFLAGS) -lxenon -lm -o default.elf

default.elf32: default.elf
> $(OBJCOPY) -O elf32-powerpc --adjust-vma 0x80000000 default.elf default.elf32
