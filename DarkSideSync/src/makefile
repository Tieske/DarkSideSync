INSTALL_DATA=cp
INSTALL_EXEC=cp
INSTALL_TOP=/opt/local

LUAINC_macosx=/opt/local/include
LUAINC_linux=/usr/include/lua5.1
LUAINC_win32="../../lua-5.1.3/src"
LUALIB_win32="../../lua-5.1.3"

#------
# Install directories
#
INSTALL_TOP_SHARE=$(INSTALL_TOP)/share/lua/5.1
INSTALL_DSS_SHARE=$(INSTALL_TOP_SHARE)/dss
INSTALL_TOP_LIB=$(INSTALL_TOP)/lib/lua/5.1
INSTALL_DSS_LIB=$(INSTALL_TOP_LIB)/dss

#------
# Supported platforms
#
PLATS= macosx linux win32

#------
# Compiler and linker settings
# for Mac OS X
SO_macosx=so
O_macosx=o
CC_macosx=gcc
DEF_macosx= -DDSS_DEBUG \
	-DSS_API='__attribute__((visibility("default")))'
CFLAGS_macosx= -I$(LUAINC) $(DEF) -pedantic -Wall -O2 -fno-common \
	-fvisibility=hidden
LDFLAGS_macosx= -bundle -undefined dynamic_lookup -o 
LD_macosx= export MACOSX_DEPLOYMENT_TARGET="10.3"; gcc

#------
# Compiler and linker settings
# for Linux
SO_linux=so
O_linux=o
CC_linux=gcc
DEF_linux=-DDSS_DEBUG \
	-DDSS_API='__attribute__((visibility("default")))'
CFLAGS_linux= -I$(LUAINC) $(DEF) -pedantic -Wall -O2 -fpic \
	-fvisibility=hidden
LDFLAGS_linux=-O -shared -fpic -o 
LD_linux=gcc

#------
# Compiler and linker settings
# for Win32
SO_win32=dll
O_win32=obj
CC_win32=cl
DEF_win32= /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" \
     /D "DSS_API=__declspec(dllexport)" /D "DSS_DEBUG" \
     /D "_CRT_SECURE_NO_WARNINGS" /D "_WINDLL"
CFLAGS_win32=/I$(LUAINC) $(DEF) /O2 /Ot /MD /W3 /nologo
LDFLAGS_win32= /nologo /link /NOLOGO /DLL /INCREMENTAL:NO \
    /LIBPATH:$(LUALIB) \
    /MANIFEST \
    /MANIFESTFILE:"intermediate.manifest" \
    /MANIFESTUAC:"level='asInvoker' uiAccess='false'" \
	/SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE:NO \
    /MACHINE:X86 ws2_32.lib lua5.1.lib /OUT:
LD_win32=cl

.SUFFIXES: .obj

.c.obj:
	$(CC) $(CFLAGS) /Fo"$@" /c $<

#------
# Output file names
#
SO=$(SO_$(PLAT))
O=$(O_$(PLAT))
DSS_V=0.1
DSS_SO=darksidesync.$(SO).$(DSS_V)
LSIGNAL_V=0.1
LSIGNAL_SO=darksidesync.$(SO).$(LSIGNAL_V)

#------
# Settings selected for platform
#
CC=$(CC_$(PLAT))
DEF=$(DEF_$(PLAT))
CFLAGS=$(CFLAGS_$(PLAT))
LDFLAGS=$(LDFLAGS_$(PLAT))
LD=$(LD_$(PLAT))
LUAINC= $(LUAINC_$(PLAT))
LUALIB= $(LUALIB_$(PLAT))

#------
# Modules belonging to DarkSideSync
#
DSS_OBJS= \
	darksidesync.$(O) \
	udpsocket.$(O) \
	locking.$(O)

LSIGNAL_OBJS= \
	luasignal.$(O)
	
#------
# Files to install
#
TO_DSS_SHARE= 

TO_TOP_SHARE= \
	dss.lua 

#------
# Targets
#
default: $(PLAT)

macosx:
	$(MAKE) all PLAT=macosx

win32:
	$(MAKE) all PLAT=win32

linux:
	$(MAKE) all PLAT=linux

none:
	@echo "Please run"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

all: $(DSS_SO)

$(DSS_SO): $(DSS_OBJS)
	$(LD) $(DSS_OBJS) $(LDFLAGS)$@ 

#$(LSIGNAL_SO): $(LSIGNAL_OBJS)
#	$(LD) $(LSIGNAL_OBJS) $(LDFLAGS)$@ 


install:
	mkdir -p $(INSTALL_TOP_SHARE)
	#$(INSTALL_DATA) $(TO_TOP_SHARE) $(INSTALL_TOP_SHARE)
	#mkdir -p $(INSTALL_DSS_SHARE)
	#$(INSTALL_DATA) $(TO_DSS_SHARE) $(INSTALL_DSS_SHARE)
	#mkdir -p $(INSTALL_DSS_LIB)
	#$(INSTALL_EXEC) $(DSS_SO) $(INSTALL_DSS_LIB)/core.$(SO)

local:
	$(MAKE) install INSTALL_TOP_LIB=.. INSTALL_TOP_SHARE=..

clean:
	rm -f $(DSS_SO) $(DSS_OBJS)

.PHONY: all $(PLATS) default clean echo none

#------
# List of dependencies
#
udpsocket.$(O): udpsocket.c udpsocket.h
locking.$(O): locking.c locking.h
darksidesync.$(O): darksidesync.c darksidesync.h locking.h udpsocket.h

