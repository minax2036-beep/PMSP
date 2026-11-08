.DEFAULT_GOAL := all

TARGET = pmsp_tester
OBJS = main.o PMSP_0000.o PMSP_0001.o PMSP_0002.o PMSP_0003.o PMSP_0004.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpspctrl -lpsputility -lpspnet_apctl -lpspnet_inet -lpspnet

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PMSP Tester
PSP_FW_VERSION = 660

PSPSDK = $(shell psp-config --pspsdk-path)

# Generate the import assembly from the PRX export definition.
PMSP.S: ../prx/exports.exp
	psp-build-exports -k $<

# PSPSDK generates one object for the import library header and one per function.
PMSP_%.o: PMSP.S
	psp-gcc $(CFLAGS) -DF_PMSP_$* -c $< -o $@

include $(PSPSDK)/lib/build.mak
