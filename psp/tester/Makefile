TARGET = pmsp_tester
OBJS = main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpspctrl

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PMSP Tester

PSP_FW_VERSION = 660

include $(PSPSDK)/lib/build.mak
