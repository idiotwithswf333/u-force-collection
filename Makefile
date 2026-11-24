# Builds msfc_wiiu.rpx and msfc_wiiu.wuhb with devkitPPC + wut + wiiu-sdl2.
# Needs: DEVKITPRO set, packages wut, wut-tools and wiiu-sdl2 installed.

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set)
endif

DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut
PORTLIBS  := $(DEVKITPRO)/portlibs/wiiu
export PATH := $(DEVKITPPC)/bin:$(DEVKITPRO)/tools/bin:$(PORTLIBS)/bin:$(PATH)

ifeq ($(wildcard $(WUT_ROOT)/share/wut.specs),)
$(error wut not found in $(WUT_ROOT). Run: pacman -S --needed wut wut-tools)
endif
ifeq ($(wildcard $(PORTLIBS)/include/SDL2/SDL.h),)
$(error SDL2 for Wii U not found. Run: pacman -S --needed wiiu-sdl2)
endif

TARGET    := msfc_wiiu
SRC       := src/main.c src/assets.c src/msfc_index.c src/adpcm.c
OBJ       := $(SRC:.c=.o)

APP_NAME  := Mighty Switch Force! Collection (WIP)
APP_SHORT := MSFC Wii U
APP_AUTH  := MSFC Wii U port
ICON      := meta/icon.png
TV_IMAGE  := meta/tv.png
DRC_IMAGE := meta/drc.png

ifneq ($(wildcard $(PORTLIBS)/bin/sdl2-config),)
SDL_CFLAGS := $(shell $(PORTLIBS)/bin/sdl2-config --cflags)
SDL_LIBS   := $(shell $(PORTLIBS)/bin/sdl2-config --libs)
else
SDL_CFLAGS := -I$(PORTLIBS)/include/SDL2
SDL_LIBS   := -L$(PORTLIBS)/lib -lSDL2
endif

ARCH    := -mcpu=750 -meabi -mhard-float
CFLAGS  := -O2 -Wall -ffunction-sections $(ARCH) -DESPRESSO -D__WIIU__ -D__WUT__ \
           -I$(WUT_ROOT)/include -I$(PORTLIBS)/include $(SDL_CFLAGS) -Isrc
LDFLAGS := $(ARCH) -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections
LIBS    := $(SDL_LIBS) -L$(WUT_ROOT)/lib -lwut

CC := powerpc-eabi-gcc

all: $(TARGET).wuhb

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# assets.c embeds these with .incbin, so rebuild it when they change
src/assets.o: meta/splash.bmp meta/loading.wav

$(TARGET).elf: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	elf2rpl $< $@

$(TARGET).wuhb: $(TARGET).rpx $(ICON) $(TV_IMAGE) $(DRC_IMAGE)
	wuhbtool $(TARGET).rpx $@ --name="$(APP_NAME)" --short-name="$(APP_SHORT)" \
	    --author="$(APP_AUTH)" --icon=$(ICON) \
	    --tv-image=$(TV_IMAGE) --drc-image=$(DRC_IMAGE)

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb

.PHONY: all clean
