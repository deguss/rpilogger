### ------------  Makefile created by Daniel Piri    2015/12/06 --   ###
###                  http://github.com/deguss                        ###
###                  http://opendatalogger.com                       ###
###     the content below is not intended to be modified by user     ###
### ------------ please edit configure.txt instead ---------------   ###

SHELL:=/bin/bash
VPATH:=src
TARGET:=ads
DEPLOY:=/opt/opendatalogger
CC:=cc  # compatibility issues (CC:=gcc-4.6)
CCFLAGS:=-std=gnu99 -O3
WFLAGS:=-Wall -Wextra -Wno-missing-field-initializers -Wfatal-errors -Wunused
LDFLAGS:=-lrt -lnetcdf -lm -lpthread -I/usr/include
OBJECTS:=main.c i2c.c system.c inifile.c filelock.c iniparser.c dictionary.c

now=$(shell date "+%F %T %z")
user=$(shell whoami)
host=$(shell hostname)
include configure.txt
mac=$(shell cat /sys/class/net/eth0/address)
PREPROC+=-D _TIMESTAMP_="\"$(now)\""
PREPROC+=-D _USER_AND_HOST="\"$(user) at $(host) from $(mac)\""

NO_COLOR=\x1b[0m
OK_COLOR=\x1b[32;01m
ERROR_COLOR=\x1b[31;01m
WARN_COLOR=\x1b[33;01m

.DEFAULT_GOAL := all	

help:
	@echo "usage:"
	@echo "    make clean        # removes output dir: build/ and debug/"
	@echo "    make all          # make all targets (use this if unsure)"
	@echo "    make ads          # make only the ads sampling program"
	@echo "    make ncglue       # make only the ncglue program"
	@echo "    make install      # install programs"
	@echo "    make uninstall    # uninstall programs"	
	@echo "    make debug        # gdb conform executable will be created for debugging"
	@echo "    make gcov         # gprof conform executable will be created for profiling"	


all: ads ncglue


ads: $(OBJECTS)
	@rm -r -f debug		
	@mkdir -p build
	@rm -f build/$(TARGET)
	
	# save gcc version information to file gccinfo
	@$(CC) -v 2> build/gccinfo
	
	# save compiler flags into file flaginfo
	@echo "$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^" > build/flaginfo
	
	# compile (and save output to file buildinfo)
	$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^ 2>&1 | tee build/buildinfo
	
	@if test -e build/$(TARGET); then \
echo -e "$(OK_COLOR)[OK] all done$(NO_COLOR)"; \
echo "to verify correct behaviour run    sudo build/ads --debug"; \
echo "if everything fine run             sudo make install"; \
else \
echo -e "$(ERROR_COLOR)compilation failed$(NO_COLOR)"; \
fi

ncglue: ncglue.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(WFLAGS) src/ncglue.c -o build/ncglue

install_ncglue: build/ncglue
ifneq ($(user),root)
	@echo -e "$(ERROR_COLOR)install requires root privileges$(NO_COLOR)"
	@exit -1
endif
	mkdir -p $(DEPLOY)
	cp build/ncglue $(DEPLOY)/
	chown pi:pi $(DEPLOY)/ncglue
	
clean: 
	@rm -r -f build
	@rm -r -f debug
	@echo "clean done"

debug: $(OBJECTS)
	@rm -r -f build
	@mkdir -p debug
	@$(CC) -E $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) $^ > debug/ads.obj
	$(CC) -g $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o debug/$(TARGET) $^
	@echo "ready for debugging. use gdb"

gcov: $(OBJECTS)
	@rm -r -f build
	@rm -r -f debug
	@mkdir -p debug
	$(CC) -fprofile-arcs -ftest-coverage $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o debug/$(TARGET) $^
	@echo "ready for coverage analysis. use gcov src/main.c and check main.c.gcov"	
	
install: build/ads build/ncglue
ifneq ($(user),root)
	@echo -e "$(ERROR_COLOR)install requires root privileges$(NO_COLOR)"
	@exit -1
endif
	@echo "installing..."
	@if [ -e /etc/init.d/opendatalogger ]; then /etc/init.d/opendatalogger stop; fi
	
	#install to /opt/opendatalogger
	mkdir -p $(DEPLOY)
	cp -R build/. $(DEPLOY)
	rm -f $(DEPLOY)/ads.conf
	chmod 0500 $(DEPLOY)/$(TARGET)

	#install init script to /etc/init.d/	
	cp ../conf/etc/init.d/opendatalogger /etc/init.d/opendatalogger
	update-rc.d opendatalogger defaults
	ls -l /etc/rc?.d/*opendatalogger
	

	#install cron.hourly job
	cp ../conf/etc/cron.hourly/cron_ncglue /etc/cron.hourly/.
	chmod +x /etc/cron.hourly/cron_ncglue
	
	@echo -e "$(OK_COLOR)[OK]$(NO_COLOR)"
	@echo "start the service by          sudo /etc/init.d/opendatalogger start"


uninstall: 
ifneq ($(user),root)
	@echo -e "$(ERROR_COLOR)uninstall requires root privileges$(NO_COLOR)"
	@exit -1
endif
	@echo "uninstalling..."
	@if [ -e /etc/init.d/opendatalogger ]; then /etc/init.d/opendatalogger stop; fi
	
	#remove /opt/opendatalogger
	rm -r $(DEPLOY)
	
	#remove init script	
	rm -f /etc/init.d/opendatalogger
	update-rc.d -f opendatalogger remove
	
	#remove cron.hourly job
	rm -f /etc/cron.hourly/cron_ncglue
	
.PHONY: all ncglue debug gcov clean install install_ncglue uninstall
	
