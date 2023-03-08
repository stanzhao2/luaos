
CONFIG := ./config
include $(CONFIG)

SRC = ./src
PREFIXPATH = $(PREFIX)/bin
THIS_DIR = $(abspath $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

LIB3RD        = ./3rd
LIBLUA        = $(LIB3RD)/lua
LIBHIREDIS    = $(LIB3RD)/hiredis/hiredis

LUAEXT        = ./lib/src
LUABIGINT     = $(LUAEXT)/lua-bigint
LUACURL       = $(LUAEXT)/lua-curl
LUAGZIP       = $(LUAEXT)/lua-gzip
LUAHIREDIS    = $(LUAEXT)/lua-hiredis
LUAHTTPPARSER = $(LUAEXT)/lua-http-parser
LUARANDOM     = $(LUAEXT)/lua-random
LUASKIPLIST   = $(LUAEXT)/lua-skiplist
LUAUUID       = $(LUAEXT)/lua-uuid
LUAXSTRING    = $(LUAEXT)/lua-xstring
LUAMYSQL      = $(LUAEXT)/lua-sql/mysql

MAKE    = make
MKDIR   = mkdir -p
RM      = rm -f
TARGET  = luaos
VERSION = 2.0
INSTALLPATH = $(PREFIXPATH)/$(TARGET).$(VERSION)

all:
	cd $(LIBLUA) && $(MAKE) linux && $(MAKE) install
	cd $(LIBHIREDIS) && $(MAKE) && $(MAKE) install
	
	$(MKDIR) ./lib
	$(MKDIR) ./lib/lua
	$(MKDIR) ./lib/lua/$(LUA_VERSION)
	$(MKDIR) ./lib/lua/$(LUA_VERSION)/http
	$(MKDIR) ./lib/lua/$(LUA_VERSION)/luasql
	
	cd $(LUABIGINT) && $(MAKE)
	cd $(LUACURL) && $(MAKE)
	cd $(LUAGZIP) && $(MAKE)
	cd $(LUAHIREDIS) && $(MAKE)
	cd $(LUAHTTPPARSER) && $(MAKE)
	cd $(LUARANDOM) && $(MAKE)
	cd $(LUASKIPLIST) && $(MAKE)
	cd $(LUAUUID) && $(MAKE)
	cd $(LUAXSTRING) && $(MAKE)
	cd $(LUAMYSQL) && $(MAKE)
	
	$(MKDIR) ./bin	
	cd $(SRC) && $(MAKE)
	
clean:
	cd $(SRC) && $(MAKE) clean
	cd $(LIBLUA) && $(MAKE) clean
	cd $(LIBHIREDIS) && $(MAKE) clean
	cd $(LUABIGINT) && $(MAKE) clean
	cd $(LUACURL) && $(MAKE) clean
	cd $(LUAGZIP) && $(MAKE) clean
	cd $(LUAHIREDIS) && $(MAKE) clean
	cd $(LUAHTTPPARSER) && $(MAKE) clean
	cd $(LUARANDOM) && $(MAKE) clean
	cd $(LUASKIPLIST) && $(MAKE) clean
	cd $(LUAUUID) && $(MAKE) clean
	cd $(LUAXSTRING) && $(MAKE) clean
	cd $(LUAMYSQL) && $(MAKE) clean
	
install:
	$(MKDIR) $(INSTALLPATH)
	$(MKDIR) $(INSTALLPATH)/bin
	$(MKDIR) $(INSTALLPATH)/lib
	
	cp -a ./lib/lua $(INSTALLPATH)/lib
	cp -a ./share $(INSTALLPATH)/
	cp ./bin/$(TARGET) $(INSTALLPATH)/bin
	
	$(RM) $(PREFIXPATH)/$(TARGET)
	ln -s $(INSTALLPATH)/bin/$(TARGET) $(PREFIXPATH)/$(TARGET)
	
tar:
	tar zcvf ./$(TARGET).$(VERSION).tar.gz $(INSTALLPATH)
	
uninstall:
	$(RM) -r $(INSTALLPATH)
	$(RM) $(PREFIXPATH)/$(TARGET)
	