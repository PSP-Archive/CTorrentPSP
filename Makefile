#CFLAGS= -DHAVE_CONFIG_H -Wall -g -G0 -I/usr/local/pspdev/psp/include -I/usr/local/pspdev/psp/sdk/include -fsingle-precision-constant
CFLAGS= -DHAVE_CONFIG_H -Wall -O2 -G0 -I/usr/local/pspdev/psp/include -I/usr/local/pspdev/psp/sdk/include -fsingle-precision-constant
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

OBJS=bencode.o bitfield.o btconfig.o btcontent.o btfiles.o btrequest.o btstream.o bufio.o compat.o connect_nonb.o console.o ctcs.o ctorrent.o downloader.o httpencode.o iplist.o peerlist.o peer.o rate.o setnonblock.o sha1.o sigint.o tracker.o

OBJS+=UI.o
BUILD_PRX = 1
TARGET = ctorrentpsp

LIBDIR =
LDFLAGS =

LIBS= -lSDL_ttf -lSDL_image -l freetype -ljpeg -lpng -lz -lm -lSDL -lpspgu -l psphprm -lpspaudio -lstdc++

PSP_EBOOT_ICON = icon0.png
PSP_EBOOT_PIC1 = pic1.png

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = CTORRENTPSP

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
