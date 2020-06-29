BUILD_BASE	= build
FW_BASE		= firmware

# base directory for the compiler

CHIP=lx106
#CHIP=esp32

XTENSA_TOOLS_ROOT ?= /home/shubin/electronic/tools/xtensa-lx106-elf/bin/xtensa-lx106-elf-

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= /home/shubin/electronic/firmware/esp/esp8266/esp_iot_sdk_v1.0.1
#SDK_BASE	?= /home/shubin/electronic/firmware/esp/esp8266/ESP8266_NONOS_SDK-3.0.4
INCLUDE_EXTRA	=

# esptool.py path and port
ESPTOOL		?= esptool.py

# name for the target project
TARGET		= app

# which modules (subdirectories) of the project to include in compiling
MODULES		= src driver

# libraries used in this project, mainly provided by the SDK
#


#LIBS		= c gcc pp phy net80211 lwip wpa main json
#LIBS		= c gcc phy pp net80211 lwip wpa main json ssl upgrade smartconfig crypto
LIBS		= c gcc phy pp net80211 lwip wpa main json ssl upgrade smartconfig
# compiler flags using during compilation of source files
#--no-warnings
CFLAGS		=  -std=c99  -Os -g -O2 -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH -Wpointer-arith -Wundef -Werror -Wno-error=implicit-function-declaration

#CFLAGS		= -Os -g -O2 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -D__ets__ -DICACHE_FLASH

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

# linker script used for the above linkier step
#LD_SCRIPT	= eagle.app.v6.ld
LD_SCRIPT	= eagle.app.v6.ld
# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json

# we create two different files for uploading into the flash
# these are the names and options to generate them
FW_FILE_1_ADDR	= 0x00000
FW_FILE_2_ADDR	= 0x10000

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)gcc
AR		:= $(XTENSA_TOOLS_ROOT)ar
LD		:= $(XTENSA_TOOLS_ROOT)gcc


####
#### no user configurable options below here
####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= -Isdk -Isdk/json

RES		:= $(wildcard res/src/*.*)
GEN		:= $(patsubst res/src/%,res/gen/%.h,$(RES))
SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

#LD_SCRIPT	:= $(addprefix -T$(SDK_BASE)/$(SDK_LDDIR)/,$(LD_SCRIPT))
LD_SCRIPT	:= $(addprefix -T,$(LD_SCRIPT))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

FW_FILE_1		:= $(addprefix $(FW_BASE)/,$(FW_FILE_1_ADDR).bin)
FW_FILE_2		:= $(addprefix $(FW_BASE)/,$(FW_FILE_2_ADDR).bin)
FW_VERION_INFO	:= $(addprefix $(FW_BASE)/,version.h)

FW_FLASHER	:= $(addprefix $(FW_BASE)/,flasher.bin)


V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(INCLUDE_EXTRA) $(SDK_INCDIR) $(CFLAGS) -c $$< -o $$@
endef

.PHONY: all checkdirs flash clean

all: checkdirs $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)

print:
	echo $(SRC)
	echo $(RES)
	echo $(GEN)

generate:
	`/bin/sh gen/exec.sh`

inc:
	echo $(SDK_INCDIR)

$(FW_BASE)/%.bin: $(TARGET_OUT) $(FW_BASE)
	$(vecho) "FW $(FW_BASE)/"
	$(Q) $(ESPTOOL) elf2image -o $(FW_BASE)/ $(TARGET_OUT)

$(FW_VERION_INFO): user/version.h
	cat user/version.h>>$(FW_VERION_INFO)

$(TARGET_OUT): gen $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_BASE):
	$(Q) mkdir -p $@

$(FW_FLASHER):

gen:

$(GEN): $(RES)
	echo $@ $^
	xxd -i $^>$@
	sed -i 's/unsigned//g' $@
	sed -i 's/res_src_//g' $@

rebuild: clean all flash_tail

boot_console:
	python /usr/lib/python3/dist-packages/serial/tools/miniterm.py $(shell ls /dev/ttyUSB*) 74880


flash: $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL) --port $(shell ls /dev/ttyUSB*) --baud 115200 write_flash $(FW_FILE_1_ADDR) $(FW_FILE_1) $(FW_FILE_2_ADDR) $(FW_FILE_2)

verfy: $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL) --port $(shell ls /dev/ttyUSB*) --baud 115200 verify_flash $(FW_FILE_1_ADDR) $(FW_FILE_1) $(FW_FILE_2_ADDR) $(FW_FILE_2)

flash_0: $(FW_FILE_1)
	$(ESPTOOL) --port $(shell ls /dev/ttyUSB*) --baud 115200 write_flash $(FW_FILE_1_ADDR) $(FW_FILE_1)

flash_1: $(FW_FILE_2)
	$(ESPTOOL) --port $(shell ls /dev/ttyUSB*) --baud 115200 write_flash $(FW_FILE_2_ADDR) $(FW_FILE_2)

flash_tail: flash
	tail -f $(shell ls /dev/ttyUSB*)

flash_tail_0: flash_0
	tail -f $(shell ls /dev/ttyUSB*)

flash_tail_1: flash_1
	tail -f $(shell ls /dev/ttyUSB*)

tail:
	tail -f $(shell ls /dev/ttyUSB*)

erase:
	$(ESPTOOL) --port $(shell ls /dev/ttyUSB*) --baud 115200 erase_flash


clean:
	$(Q) rm -rf $(FW_BASE) $(BUILD_BASE) $(GEN)


$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))

.PHONY: all
