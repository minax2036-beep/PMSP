TARGET = pmsp
OBJS = main.o

BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSP_FW_VERSION = 660

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -Wextra
ASFLAGS = $(CFLAGS)
LDFLAGS = -nostartfiles

EXTRA_TARGETS = $(TARGET).prx

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
