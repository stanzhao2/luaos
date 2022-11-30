SUBDIR0  = ./src/lua-skiplist
SUBDIR1  = ./src/lua-bigint
SUBDIR2  = ./src/lua-cjson
SUBDIR3  = ./src/lua-conv
SUBDIR4  = ./src/lua-curl
SUBDIR5  = ./src/lua-random
SUBDIR6  = ./src/lua-rapidjson
SUBDIR7  = ./src/lua-pack
SUBDIR8  = ./src/lua-gzip
SUBDIR9  = ./src/lua-http-parser
SUBDIR10 = ./src/lua-sql

MAKE = make

subsystem:
	mkdir -p ../bin
	mkdir -p ../bin/sys
	mkdir -p ../bin/sys/http
	mkdir -p ../bin/sys/luasql
	
	cd $(SUBDIR0)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR1)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR2)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR3)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR4)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR5)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR6)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR7)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR8)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR9)  && $(MAKE) clean && $(MAKE)
	cd $(SUBDIR10) && $(MAKE) clean && $(MAKE) mysql
	
	rm -f ../bin/sys/luasql/mysql.so
	cp $(SUBDIR10)/src/mysql.so ../bin/sys/luasql
		
	@echo "==== Successfully built extensions ===="
