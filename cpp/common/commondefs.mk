#-----------------------------------------------------------------------------
# commondefs.mk               						
#
# Description: 
#       Common definitions for building against libraries that may or
#	may not be installed in the system.
#	It detects presence and sets up compiler definitions for various
#	libraries :
#		libtiff	-> used for TIFF file handling
#		libpng	-> used for PNG file handling
#		libjpeg	-> used for JPEG file handling
#		freeGlut	-> used for GL display modes (original glut is no longer maintained)
#		x11dev	-> used for X11 display modes
#
#-----------------------------------------------------------------------------#
# 
#
ifneq ("$(shell find /usr/include -iname tiffio.h -print)","")
	COMMON_OPTIONS += -DLIBTIFF_AVAILABLE
	COMMONLIBS += -L/usr/lib -ltiff
endif

ifneq ("$(shell find /usr/include -iname jpeglib.h -print)","")
	COMMON_OPTIONS += -DLIBJPEG_AVAILABLE
endif

ifneq ("$(shell find /usr/include -iname png.h -print)","")
	COMMON_OPTIONS += -DLIBPNG_AVAILABLE
endif

ifneq ("$(shell find /usr/include -iname xlib.h -print)","")
	COMMON_OPTIONS += -DX11_DISPLAY_AVAILABLE
endif

ifneq ("$(shell find /usr/include -iname freeglut.h -print)","")
	COMMON_OPTIONS += -DFREEGLUT_DISPLAY_AVAILABLE
endif

