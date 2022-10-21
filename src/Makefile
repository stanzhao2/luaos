
###################################################################

CCOMPILE   := gcc
CPPCOMPILE := g++
LINK       := g++

CONFIG := ../config

include $(CONFIG)

######################### OPTIONS BEGIN ###########################
#target file

TARGET_NAME = luaos

OUTPUT  := ../bin/$(TARGET_NAME)

#source files
SOURCE  := ../src/twheel/twheel.o \
           ../src/luaos.o \
		   ../src/luaos_job.o \
		   ../src/luaos_loader.o \
		   ../src/luaos_printf.o \
		   ../src/luaos_socket.o \
		   ../src/luaos_storage.o \
		   ../src/luaos_subscriber.o \
		   ../src/luaos_thread_local.o \
		   ../src/luaos_twheel.o \
		   ../src/main.o
		   
MINIZIP_DIR := ../3rd/zlib/contrib/minizip

#library path
LIBDIRS := -L$(LUA_LIBDIR) -L$(PREFIX)/lib -L$(MINIZIP_DIR)

#include path
INCDIRS := $(LUA_INCLUDE) -I$(PREFIX)/include -I$(MINIZIP_DIR) -I../include -I../3rd/asio

#precompile macro
CC_FLAG :=

#compile options
COMPILEOPTION := -std=c++11 -fPIC -w -Wfatal-errors -O3 -pipe -m64

#rpath options
LIBLOADPATH := -Wl,-rpath=./ -Wl,-rpath=./lib -Wl,-rpath=/usr/local/lib

#link options
LINKOPTION := -o $(OUTPUT) $(LIBLOADPATH)

#dependency librarys
LIBS := -llua -lunz -lz -ldl -lpthread -ltcmalloc

########################## OPTIONS END ############################

$(OUTPUT): $(SOURCE)
	$(LINK) $(LINKOPTION) $(LIBDIRS) $(SOURCE) $(LIBS)

clean: 
	rm -f $(SOURCE)
	rm -f $(OUTPUT)

all: clean $(OUTPUT)
.PRECIOUS:%.cpp %.c %.C
.SUFFIXES:
.SUFFIXES:  .c .o .cpp .ecpp .pc .ec .C .cc .cxx

.cpp.o:
	$(CPPCOMPILE) -c -o $*.o $(CC_FLAG) $(COMPILEOPTION) $(INCDIRS)  $*.cpp
	
.cc.o:
	$(CCOMPILE) -c -o $*.o $(CC_FLAG) $(COMPILEOPTION) $(INCDIRS)  $*.cx

.cxx.o:
	$(CPPCOMPILE) -c -o $*.o $(CC_FLAG) $(COMPILEOPTION) $(INCDIRS)  $*.cxx

.c.o:
	$(CCOMPILE) -c -o $*.o $(CC_FLAG) $(COMPILEOPTION) $(INCDIRS) $*.c

.C.o:
	$(CPPCOMPILE) -c -o $*.o $(CC_FLAG) $(COMPILEOPTION) $(INCDIRS) $*.C

########################## MAKEFILE END ############################
