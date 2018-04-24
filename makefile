#INCLUDES = -I/usr/include/opencv  
#LIBS = -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_ml  
#LIBDIRS = -L/usr/lib  

CC= gcc
# IROOT directory based on installed distribution tree (not archive/development tree). 
# IROOT=../..
IROOT=/usr/dalsa/GigeV
QTROOT=/usr/include/aarch64-linux-gnu/qt5

#
# Get the configured include defs file.
# (It gets installed to the distribution tree).
ifeq ($(shell if test -e archdefs.mk; then echo exists; fi), exists)
	include archdefs.mk
else
# Force an error
$(error	archdefs.mk file not found. It gets configured on installation ***)
endif

INC_PATH = -I. -I$(QTROOT) -I$(QTROOT)/QtCore -I$(QTROOT)/QtGui -I$(IROOT)/include -I$(IROOT)/examples/common $(INC_GENICAM) -I/usr/include/opencv -I/usr/include/opencv2
                          
DEBUGFLAGS = -g 

CXX_COMPILE_OPTIONS = -c $(DEBUGFLAGS) -DPOSIX_HOSTPC -D_REENTRANT -fno-for-scope \
			-Wall -Wno-parentheses -Wno-missing-braces -Wno-unused-but-set-variable \
			-Wno-unknown-pragmas -Wno-cast-qual -Wno-unused-function -Wno-unused-label

C_COMPILE_OPTIONS= $(DEBUGFLAGS) -fhosted -Wall -Wno-parentheses -Wno-missing-braces \
		   	-Wno-unknown-pragmas -Wno-cast-qual -Wno-unused-function -Wno-unused-label -Wno-unused-but-set-variable


LCLLIBS=  -L$(ARCHLIBDIR) -lpthread -lXext -lX11 -L/usr/local/lib -L/usr/lib -lGevApi -lCorW32 -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_ml -L/usr/lib/aarch64-linux-gnu -lQt5Network -lQt5Core -lQt5Gui

VPATH= . : ../common

%.o : %.cpp
	$(CC) -I. $(INC_PATH) $(CXX_COMPILE_OPTIONS) $(ARCH_OPTIONS) -c $< -o $@

%.o : %.c
	$(CC) -I. $(INC_PATH) $(C_COMPILE_OPTIONS) $(ARCH_OPTIONS) -c $< -o $@

OBJS= genicam_cpp_demo.o \
      GevUtils.o \
      X_Display_utils.o

genicam_cpp_demo : $(OBJS)
	$(CC) -g $(ARCH_LINK_OPTIONS) -o genicam_cpp_demo $(OBJS) $(LCLLIBS) $(GENICAM_LIBS) -L$(ARCHLIBDIR) -lstdc++

clean:
	rm *.o genicam_cpp_demo 


