# Builds msfc_wiiu.rpx and msfc_wiiu.wuhb with devkitPPC + wut.
# Needs: DEVKITPRO set, packages wut and wut-tools installed.

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set)
endif

DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
export PATH := $(DEVKITPPC)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

ifeq ($(wildcard $(WUT_ROOT)/share/wut.specs),)
$(error wut not found in $(WUT_ROOT). Run: pacman -S --needed wut wut-tools)
endif

TARGET    := msfc_wiiu
SRC       := src/main.c src/msfc_index.c
OBJ       := $(SRC:.c=.o)

APP_NAME  := Mighty Switch Force! Collection (WIP)
APP_SHORT := MSFC Wii U
APP_AUTH  := MSFC Wii U port
ICON      := meta/icon.png

ARCH    := -mcpu=750 -meabi -mhard-float
CFLAGS  := -O2 -Wall -ffunction-sections $(ARCH) -DESPRESSO -D__WIIU__ -D__WUT__ \
           -I$(WUT_ROOT)/include -Isrc
LDFLAGS := $(ARCH) -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections
LIBS    := -L$(WUT_ROOT)/lib -lwut

CC := powerpc-eabi-gcc

all: $(TARGET).wuhb

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	elf2rpx $< $@

$(TARGET).wuhb: $(TARGET).rpx $(ICON)
	wuhbtool $(TARGET).rpx $@ --name="$(APP_NAME)" --short-name="$(APP_SHORT)" \
	    --author="$(APP_AUTH)" --icon=$(ICON)

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb

.PHONY: all clean
