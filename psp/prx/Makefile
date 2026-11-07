TARGET = pmsp
OBJS = main.o

BUILD_PRX = 1
PSP_FW_VERSION = 660

CFLAGS = -O2 -G0 -Wall -Wextra
ASFLAGS = $(CFLAGS)

EXTRA_TARGETS = $(TARGET).prx

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
