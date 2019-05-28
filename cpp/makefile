CC= gcc
# IROOT directory based on installed distribution tree (not archive/development tree). 
# IROOT=../..
IROOT=/usr/dalsa/GigeV

#
# Get the configured include defs file (required for direct GenApi access)
# (It gets installed to the distribution tree).
ifeq ($(shell if test -e archdefs.mk; then echo exists; fi), exists)
	include archdefs.mk
else
# Force an error
$(error	archdefs.mk file not found. It gets configured on installation ***)
endif

INC_PATH = -I. -I$(IROOT)/include -I$(IROOT)/examples/common $(INC_GENICAM)
                          
DEBUGFLAGS = -g 

#
# Conditional definitions for the common demo files
# (They depend on libraries installed in the system).
#
include ./common/commondefs.mk

CXX_COMPILE_OPTIONS = -c $(DEBUGFLAGS) -DPOSIX_HOSTPC -D_REENTRANT -fno-for-scope \
			-Wall -Wno-parentheses -Wno-missing-braces -Wno-unused-but-set-variable \
			-Wno-unknown-pragmas -Wno-cast-qual -Wno-unused-function -Wno-unused-label

C_COMPILE_OPTIONS= $(DEBUGFLAGS) -fhosted -Wall -Wno-parentheses -Wno-missing-braces \
		   	-Wno-unknown-pragmas -Wno-cast-qual -Wno-unused-function -Wno-unused-label -Wno-unused-but-set-variable


LCLLIBS=  -L$(ARCHLIBDIR) $(COMMONLIBS) -lpthread -lXext -lX11 -L/usr/local/lib -lGevApi -lCorW32

VPATH= . : ./common

%.o : %.cpp
	$(CC) -I. $(INC_PATH) $(CXX_COMPILE_OPTIONS) $(COMMON_OPTIONS) $(ARCH_OPTIONS) -c $< -o $@

%.o : %.c
	$(CC) -I. $(INC_PATH) $(C_COMPILE_OPTIONS) $(COMMON_OPTIONS) $(ARCH_OPTIONS) -c $< -o $@

OBJS= genicam.o \
      GevUtils.o \
      convertBayer.o \
      GevFileUtils.o \
      FileUtil_tiff.o \
      X_Display_utils.o

genicam : $(OBJS)
	$(CC) -g $(ARCH_LINK_OPTIONS) -o genicam $(OBJS) $(LCLLIBS) $(GENICAM_LIBS) -L$(ARCHLIBDIR) -lstdc++

clean:
	rm *.o genicam 


