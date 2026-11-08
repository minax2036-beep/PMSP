.DEFAULT_GOAL := all

TARGET = pmsp_tester
OBJS = main.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpspctrl -lpsputility -lpspsdk -lpspnet_apctl -lpspnet_inet -lpspnet

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PMSP Net Init Test
PSP_FW_VERSION = 660

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
