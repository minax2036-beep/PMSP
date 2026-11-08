TARGET = pmsp_tester
OBJS = main.o

CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PMSP Netconf PRX Loader
PSP_FW_VERSION = 660

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
