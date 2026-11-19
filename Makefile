TARGET		:= GeometryDash10
BUILD		:= build
SOURCES		:= source
DATA		:= data
INCLUDES	:= include

APP_TITLE	:= Geometry Dash 1.0
APP_DESCRIPTION	:= Rhythm-based action platformer for 3DS
APP_AUTHOR	:= HomebrewDev
APP_PRODUCT_CODE := CTR-P-GD10
APP_UNIQUE_ID	:= 0x0GD10

DEVKITARM	?= /opt/devkitpro/devkitARM
DEVKITPRO	?= /opt/devkitpro

PREFIX		:= arm-none-eabi-
CC		:= $(DEVKITARM)/bin/$(PREFIX)gcc
CXX		:= $(DEVKITARM)/bin/$(PREFIX)g++
AS		:= $(DEVKITARM)/bin/$(PREFIX)as
LD		:= $(DEVKITARM)/bin/$(PREFIX)g++

export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)

ARCH	:= -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS	:= -g -Wall -O3 -mword-relocations -fomit-frame-pointer -ffunction-sections $(ARCH) $(DEFINES) -I$(DEVKITPRO)/libctru/include -I$(DEVKITPRO)/portlibs/3ds/include
CXXFLAGS:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS	:= -g $(ARCH)
LDFLAGS	:= -specs=3dsx.specs -g $(ARCH) -L$(DEVKITPRO)/libctru/lib -L$(DEVKITPRO)/portlibs/3ds/lib -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lctru -lm

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
OFILES   := $(addprefix $(BUILD)/,$(notdir $(CPPFILES:.cpp=.o)))

.PHONY: all clean cia

all: $(TARGET).3dsx

cia: $(TARGET).cia

$(TARGET).cia: $(TARGET).elf
	@bannertool makebanner -i assets/banner.png -a assets/audio.wav -o $(BUILD)/banner.bnr
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i assets/icon.png -o $(BUILD)/icon.smdh
	@makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf build.rsf -icon $(BUILD)/icon.smdh -banner $(BUILD)/banner.bnr
	@echo "CIA generado: $(TARGET).cia"

$(TARGET).elf: $(OFILES)
	$(LD) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

$(BUILD)/%.o: $(SOURCES)/%.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).cia
