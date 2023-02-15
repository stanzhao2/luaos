
CONFIG := ./config
include $(CONFIG)

SRC = ./src
INSTALLPATH = $(PREFIX)/bin
THIS_DIR = $(abspath $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

LIB3RD        = ./3rd
LIBLUA        = $(LIB3RD)/lua
LIBHIREDIS    = $(LIB3RD)/hiredis/hiredis

LUAEXT        = ./lib/src
LUABIGINT     = $(LUAEXT)/lua-bigint
LUACJSON      = $(LUAEXT)/lua-cjson
LUACONV       = $(LUAEXT)/lua-conv
LUACURL       = $(LUAEXT)/lua-curl
LUAGZIP       = $(LUAEXT)/lua-gzip
LUAHIREDIS    = $(LUAEXT)/lua-hiredis
LUAHTTPPARSER = $(LUAEXT)/lua-http-parser
LUAPACK       = $(LUAEXT)/lua-pack
LUARANDOM     = $(LUAEXT)/lua-random
LUARAPIDJSON  = $(LUAEXT)/lua-rapidjson
LUASKIPLIST   = $(LUAEXT)/lua-skiplist
LUAUUID       = $(LUAEXT)/lua-uuid
LUAMYSQL      = $(LUAEXT)/lua-sql/mysql

MAKE   = make
MKDIR  = mkdir -p
RM     = rm -f
TARGET = luaos

all:
	cd $(LIBLUA) && $(MAKE) linux && $(MAKE) install
	cd $(LIBHIREDIS) && $(MAKE) && $(MAKE) install
	
	$(MKDIR) ./lib
	$(MKDIR) ./lib/lua
	$(MKDIR) ./lib/lua/$(LUA_VERSION)
	$(MKDIR) ./lib/lua/$(LUA_VERSION)/http
	$(MKDIR) ./lib/lua/$(LUA_VERSION)/luasql
	
	cd $(LUABIGINT) && $(MAKE)
	cd $(LUACJSON) && $(MAKE)
	cd $(LUACONV) && $(MAKE)
	cd $(LUACURL) && $(MAKE)
	cd $(LUAGZIP) && $(MAKE)
	cd $(LUAHIREDIS) && $(MAKE)
	cd $(LUAHTTPPARSER) && $(MAKE)
	cd $(LUAPACK) && $(MAKE)
	cd $(LUARANDOM) && $(MAKE)
	cd $(LUARAPIDJSON) && $(MAKE)
	cd $(LUASKIPLIST) && $(MAKE)
	cd $(LUAUUID) && $(MAKE)
	cd $(LUAMYSQL) && $(MAKE)
	
	$(MKDIR) ./bin	
	cd $(SRC) && $(MAKE)
	@echo "==== Successfully built LuaOS ===="
	
clean:
	cd $(SRC) && $(MAKE) clean
	cd $(LIBLUA) && $(MAKE) clean
	cd $(LIBHIREDIS) && $(MAKE) clean
	cd $(LUABIGINT) && $(MAKE) clean
	cd $(LUACJSON) && $(MAKE) clean
	cd $(LUACONV) && $(MAKE) clean
	cd $(LUACURL) && $(MAKE) clean
	cd $(LUAGZIP) && $(MAKE) clean
	cd $(LUAHIREDIS) && $(MAKE) clean
	cd $(LUAHTTPPARSER) && $(MAKE) clean
	cd $(LUAPACK) && $(MAKE) clean
	cd $(LUARANDOM) && $(MAKE) clean
	cd $(LUARAPIDJSON) && $(MAKE) clean
	cd $(LUASKIPLIST) && $(MAKE) clean
	cd $(LUAUUID) && $(MAKE) clean
	cd $(LUAMYSQL) && $(MAKE) clean
	
install:
	$(RM) $(INSTALLPATH)/$(TARGET)
	ln -s $(THIS_DIR) $(INSTALLPATH)/$(TARGET)
	@echo "==== Successfully install LuaOS ===="
	