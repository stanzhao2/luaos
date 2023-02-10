SUBDIR1 = ./3rd/lua
SUBDIR2 = ./src

MAKE = make

subsystem:
	mkdir -p ./bin
	
	cd $(SUBDIR1) && $(MAKE) clean && $(MAKE) linux && $(MAKE) install

	cd $(SUBDIR2) && $(MAKE) clean && $(MAKE) $(SSL)
	
	@echo "==== Successfully built LuaOS ===="
	