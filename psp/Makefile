TARGET = NooDS

PSP_LARGE_MEMORY = 1
SRCDIR = ..

OBJS = $(SRCDIR)/cartridge.o \
$(SRCDIR)/core.o \
$(SRCDIR)/cp15.o \
$(SRCDIR)/div_sqrt.o \
$(SRCDIR)/dma.o \
$(SRCDIR)/gpu.o \
$(SRCDIR)/gpu_2d.o \
$(SRCDIR)/gpu_3d.o \
$(SRCDIR)/gpu_3d_renderer.o \
$(SRCDIR)/input.o \
$(SRCDIR)/interpreter.o \
$(SRCDIR)/ipc.o \
$(SRCDIR)/memory.o \
$(SRCDIR)/rtc.o \
$(SRCDIR)/settings.o \
$(SRCDIR)/spi.o \
$(SRCDIR)/timers.o \
$(SRCDIR)/spu.o \
$(SRCDIR)/wifi.o \
	  GPU/draw.o \
	  	  GUI.o  \
		  main.o


CC=psp-g++
CXX=psp-g++

PSPSDK = $(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config -d)

CFLAGS = -O3 -finline-functions -fno-rtti -ffast-math -fweb -fomit-frame-pointer -Wall -D PSP -funsigned-char -Wall  -falign-loops -falign-labels -falign-jumps -faligned-new #-D DEBUG -G0 -D DEBUG

CXXFLAGS = $(CFLAGS) -fno-exceptions

ASFLAGS = $(CFLAGS) 

INCDIR =  include/

LIBDIR =  libs/
LIBS  =  -lME -lpspdmac -lpspirkeyb -lpspwlan -lpspgum -lpspgu -lpsppower -lfreetype -lm -lpsphprm -lpspaudio -lstdc++ -lpspvfpu -lpsprtc -lvorbisidec
LIBS +=  -lpspaudiolib -lvorbisidec -logg

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = NooDS

include $(PSPSDK)/lib/build.mak

#PSP_EBOOT_ICON = ICON0.png
#PSP_EBOOT_PIC1 = PIC1.png
#PSP_EBOOT_SND0 = SND0.at3
