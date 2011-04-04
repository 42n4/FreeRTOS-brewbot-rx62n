all : freertos.elf

PYMITE=pymite-09
PM_PLAT = $(PYMITE)/src/platform/rx
PM_LIB  = $(PYMITE)/src/vm/libpmvm_rx.a

CFLAGS = \
	-D__DEBUG__=1 -fno-strict-aliasing -gstabs -ggdb \
	-I brewbot \
	-I brewbot/drivers \
	-I brewbot/include \
	-I brewbot/Common/include \
	-I brewbot/network-apps \
	-I brewbot/network-apps/webserver \
	-I brewbot/Common/ethernet/FreeTCPIP \
	-I Source/include \
	-I Source/portable/GCC/RX600 \
	-I pymite-09/src/vm \
	-I pymite-09/src/platform/rx \
	$(END)


CFILES_ENET = \
	brewbot/GNU-Files/start.asm \
	brewbot/GNU-Files/hwinit.c \
	brewbot/GNU-Files/inthandler.c \
	brewbot/Renesas-Files/hwsetup.c \
	Source/list.c \
	Source/portable/GCC/RX600/port.c \
	Source/portable/MemMang/heap_2.c \
	Source/queue.c \
	Source/tasks.c \
	brewbot/Common/ethernet/FreeTCPIP/apps/httpd/http-strings.c \
	brewbot/Common/ethernet/FreeTCPIP/apps/httpd/httpd-fs.c \
	brewbot/Common/ethernet/FreeTCPIP/apps/httpd/httpd.c \
	brewbot/Common/ethernet/FreeTCPIP/psock.c \
	brewbot/Common/ethernet/FreeTCPIP/timer.c \
	brewbot/Common/ethernet/FreeTCPIP/uip.c \
	brewbot/Common/ethernet/FreeTCPIP/uip_arp.c \
	brewbot/network-apps/webserver/EMAC.c \
	brewbot/network-apps/webserver/httpd-cgi.c \
	brewbot/network-apps/webserver/phy.c \
	brewbot/network-apps/memb.c \
	brewbot/network-apps/ftpd.c \
	brewbot/network-apps/telnetd.c \
	brewbot/network-apps/shell.c \
	brewbot/network-apps/shell_io.c \
	brewbot/network-apps/socket_io.c \
	brewbot/network-apps/uIP_Task.c \
	brewbot/drivers/ds1820.c \
	brewbot/drivers/spi.c \
	brewbot/drivers/serial.c \
	brewbot/drivers/audio.c \
	brewbot/drivers/p5q.c \
	brewbot/drivers/lcd.c \
	brewbot/drivers/font_x5x7.c \
	brewbot/drivers/vects.c \
	brewbot/fatfs/ff.c \
	brewbot/fatfs/diskio.c \
	brewbot/hop_droppers.c \
	brewbot/logging.c \
	brewbot/crane.c \
	brewbot/menu.c \
	brewbot/fill.c \
	brewbot/diagnostics.c \
	brewbot/heat.c \
	brewbot/brew.c \
	brewbot/buttons.c \
	brewbot/level_probes.c \
	brewbot/brew_task.c \
	brewbot/settings.c \
	brewbot/recipes.c \
	brewbot/main-full.c \
	pymite-09/src/platform/rx/main.c \
	pymite-09/src/platform/rx/main_nat.c \
	pymite-09/src/platform/rx/main_img.c \
	pymite-09/src/platform/rx/plat.c

CFILES = \
	brewbot/GNU-Files/start.asm \
	brewbot/main-blinky.c \
	brewbot/ParTest.c \
	brewbot/vects.c \
	Source/list.c \
	Source/queue.c \
	Source/tasks.c \
	Source/portable/MemMang/heap_2.c \
	Source/portable/GCC/RX600/port.c \
	brewbot/GNU-Files/hwinit.c \
	brewbot/GNU-Files/inthandler.c \
	brewbot/Renesas-Files/hwsetup.c \
	$(END)

OFILES := $(addsuffix .o,$(basename $(CFILES_ENET)))

freertos.elf : $(PM_PLAT)/pmfeatures.h $(OFILES) $(PM_LIB)
	rx-elf-gcc -nostartfiles $(OFILES) $(PM_LIB) -o freertos.elf -T RTOSDemo_Blinky_auto.gsi -lm
	rx-elf-size freertos.elf

%.o : %.c
	rx-elf-gcc -Wall -c $(CFLAGS) -Os $< -o $@

%.o : %.S
	rx-elf-gcc -x assembler-with-cpp -c $(CFLAGS) -O2 $< -o $@

%.o : %.asm
	rx-elf-gcc -x assembler-with-cpp -c $(CFLAGS) -O2 $< -o $@

flash : freertos.elf
	sudo rxusb -v freertos.elf

PM_USR_SOURCES = $(PM_PLAT)/main.py
PMIMGCREATOR := $(PYMITE)/src/tools/pmImgCreator.py
PMGENPMFEATURES := $(PYMITE)/src/tools/pmGenPmFeatures.py
TARGET=$(PM_PLAT)/main
# Generate native code and module images from the python source
$(TARGET)_nat.c $(TARGET)_img.c: $(PM_USR_SOURCES) $(PM_PLAT)/pmfeatures.py
	$(PMIMGCREATOR) -f $(PM_PLAT)/pmfeatures.py -c -u -o $(TARGET)_img.c --native-file=$(TARGET)_nat.c $(PM_USR_SOURCES)

$(PM_PLAT)/pmfeatures.h : $(PM_PLAT)/pmfeatures.py $(PMGENPMFEATURES)
	$(PMGENPMFEATURES) $(PM_PLAT)/pmfeatures.py > $@


PMSTDLIB_SOURCES = $(PYMITE)/src/lib/list.py \
                   $(PYMITE)/src/lib/dict.py \
                   $(PYMITE)/src/lib/__bi.py \
                   $(PYMITE)/src/lib/sys.py \
                   $(PYMITE)/src/lib/string.py
ifeq ($(IPM),true)
	PMSTDLIB_SOURCES += ../lib/ipm.py
endif


SOURCE_IMG := $(PYMITE)/src/vm/pmstdlib_img.c
SOURCE_NAT := $(PYMITE)/src/vm/pmstdlib_nat.c

ARFLAGS = rcs
SOURCES = $(wildcard $(PYMITE)/src/vm/*.c) $(SOURCE_IMG) $(SOURCE_NAT)
OBJECTS = $(SOURCES:.c=.o)

# The archive is generated by placing object files inside it
$(PM_LIB) : $(PM_LIB)($(OBJECTS))

# Build the standard library into an image file and native function file
$(SOURCE_IMG) $(SOURCE_NAT) : $(PMSTDLIB_SOURCES) $(PMIMGCREATOR) $(PM_PLAT)/pmfeatures.py
	$(PMIMGCREATOR) -f $(PM_PLAT)/pmfeatures.py -c -s --memspace=flash -o $(SOURCE_IMG) --native-file=$(SOURCE_NAT) $(PMSTDLIB_SOURCES)

blah:
	echo $(OBJECTS)


clean :
	rm -f $(OFILES) freertos.elf $(OBJECTS) $(PM_LIB) $(SOURCE_IMG) $(SOURCE_NAT) $(PM_PLAT)/pmfeatures.h


