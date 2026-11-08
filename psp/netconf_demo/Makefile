TARGET = netdialog_demo
OBJS = main.o

BUILD_PRX = 1
PSP_FW_VERSION = 660

CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpsputility -lpspsdk -lpspnet_apctl -lpspnet_inet -lpspnet -lpspgum -lpspgu

EXTRA_TARGETS = $(TARGET).prx
PSP_EBOOT_TITLE = PMSP Netconf Demo PRX

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
