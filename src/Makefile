
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
SOURCE  := luaos.o \
		   luaos_compile.o \
           luaos_logo.o \
		   luaos_socket.o \
		   luaos_rpc.o \
		   luaos_mc.o \
		   luaos_local.o \
		   luaos_state.o \
		   luaos_storage.o \
		   luaos_subscriber.o \
		   luaos_traceback.o
		   
#library path
LIBDIRS := -L$(LUA_LIBDIR)

#include path
INCDIRS := $(LUA_INCLUDE) -I../include

#precompile macro
CC_FLAG := -DTLS_SSL_ENABLE

#compile options
COMPILEOPTION := -std=c++11 -fPIC -w -Wfatal-errors -O2

#rpath options
LIBLOADPATH := -Wl,-rpath=./ -Wl,-rpath=../lib -Wl,-rpath=L$(LUA_LIBDIR)

#link options
LINKOPTION := -o $(OUTPUT) $(LIBLOADPATH)

#dependency librarys
LIBS := -llua -ldl -lcrypto -lssl -lpthread -ltcmalloc

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
