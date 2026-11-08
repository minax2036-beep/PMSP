TARGET = pmsp_tester
OBJS = main.o PMSP.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpspctrl

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PMSP Tester
PSP_FW_VERSION = 660

PSPSDK = $(shell psp-config --pspsdk-path)

# Generate the standard weak-import stubs from the PRX export definition.
PMSP.S: ../prx/exports.exp
	psp-build-exports -k ../prx/exports.exp

include $(PSPSDK)/lib/build.mak
