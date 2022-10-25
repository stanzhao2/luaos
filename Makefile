SUBDIR1 = ./3rd/lua
SUBDIR2 = ./3rd/zlib
SUBDIR3 = ./3rd/zlib/contrib/minizip
SUBDIR4 = ./src

MAKE = make

subsystem:
	mkdir -p ./bin
	
	cd $(SUBDIR1) && $(MAKE) clean && $(MAKE) linux && $(MAKE) install
	
	cd $(SUBDIR2) && ./configure && $(MAKE) clean && $(MAKE) install
	
	cd $(SUBDIR3) && $(MAKE) clean && $(MAKE)
	
	cd $(SUBDIR4) && $(MAKE) clean && $(MAKE) $(SSL)
	
	@echo "==== Successfully built LuaOS ===="
	