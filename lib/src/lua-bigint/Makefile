
###################################################################

CCOMPILE   := gcc
CPPCOMPILE := g++
LINK       := g++

CONFIG=../../../config

include $(CONFIG)

######################### OPTIONS BEGIN ###########################
#target file
OUTPUT  := ../../lua/$(LUA_VERSION)/bigint.so

#source files
SOURCE  := src/lua-bigint.o src/bigint/bigint.o src/bigint/bigint-glue.o

#library path
LIBDIRS := -L$(LUA_LIBDIR)

#include path
INCDIRS := $(LUA_INCLUDE) -I../../../include

#precompile macro
CC_FLAG :=

#compile options
COMPILEOPTION := -std=c++11 -fPIC -w -Wfatal-errors -O2

#link options
LINKOPTION := -shared -Wl,--retain-symbols-file=./src/symbol/bigint.export -Wl,--version-script=./src/symbol/bigint.script -o $(OUTPUT)

#dependency librarys
LIBS := -llua

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
