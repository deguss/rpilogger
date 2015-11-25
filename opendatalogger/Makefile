# config I2C addresses
# valid settings: 0x48, 0x49, 0x4A, 0x4B
PREPROC+=-D CFG_ADC1=0x49
PREPROC+=-D CFG_ADC2=0x48

# config sampling sequence
# valid settings: ADS_CONF_MUX_D_01, ADS_CONF_MUX_D_23, etc. see in ads1115.h
# (do not get confused if you only need 2 channels and the sequence number is not continous)
PREPROC+=-D CFG_SEQ1_ADC1=ADS_CONF_MUX_D_01
PREPROC+=-D CFG_SEQ2_ADC2=ADS_CONF_MUX_D_01
#PREPROC+=-D CFG_SEQ3_ADC1=ADS_CONF_MUX_D_23
#PREPROC+=-D CFG_SEQ4_ADC2=ADS_CONF_MUX_D_23




### ------------  Makefile created by Daniel Piri  ---------------   ###
###                  http://github.com/deguss                        ###
###                  http://opendatalogger.com                       ###
### --- the content below is not intended to be modified by user --- ###
SHELL:=/bin/bash
VPATH:=src
TARGET:=ads
DEPLOY:=/opt/opendatalogger
CC:=gcc-4.6
CCFLAGS:=-std=gnu99 -O3
WFLAGS:=-Wall -Wextra -Wno-missing-field-initializers -Wfatal-errors -Wunused
LDFLAGS:=-lrt -lnetcdf -lm -lpthread
OBJECTS:=main.c i2c.c system.c inifile.c filelock.c iniparser.c dictionary.c

now=$(shell date "+%F %T %z")
user=$(shell whoami)
host=$(shell hostname)
mac=$(shell cat /sys/class/net/eth0/address)
PREPROC+=-D _TIMESTAMP_="\"$(now)\""
PREPROC+=-D _USER_AND_HOST="\"$(user) at $(host) from $(mac)\""

NO_COLOR=\x1b[0m
OK_COLOR=\x1b[32;01m
ERROR_COLOR=\x1b[31;01m
WARN_COLOR=\x1b[33;01m
	
		
all: $(OBJECTS)
	@rm -r -f debug		
	@mkdir -p build
	@rm -f build/$(TARGET)
# save gcc version information to file gccinfo
	@$(CC) -v 2> build/gccinfo
# save compiler flags into file flaginfo
	@echo "$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^" > build/flaginfo
# compile (and save output to file buildinfo
	$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^ 2>&1 | tee build/buildinfo
	@if test -e build/$(TARGET); then \
echo -e "$(OK_COLOR)[OK] all done$(NO_COLOR)"; \
echo "to verify correct behaviour run    sudo build/ads --debug"; \
echo "if everything fine run             sudo make install"; \
else \
echo -e "$(ERROR_COLOR)compilation failed$(NO_COLOR)"; \
fi

help:
	@echo "usage:"
	@echo "        make clean           # removes output dir: build/ and debug/"
	@echo "        make all             # binary executable build/$(TARGET) will be created"
	@echo "        make install         # no effect"
	@echo "        make uninstall       # no efffect"	
	@echo "        make debug           # gdb conform executable will be created for debugging"
	@echo "        make gcov            # gprof conform executable will be created for profiling"	

clean: 
	@rm -r -f build
	@rm -r -f debug
	@rm -r -f log
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
	
install: build
ifneq ($(user),root)
	@echo -e "$(ERROR_COLOR)make sure you have root privilegs prior installing$(NO_COLOR)"
	@exit -1
endif
	@echo "installing..."
	test -e /etc/init.d/opendatalogger && /etc/init.d/opendatalogger stop	
	
#install to /opt/opendatalogger
	mkdir -p $(DEPLOY)
	cp build/$(TARGET) $(DEPLOY)/$(TARGET)
	cp build/buildinfo $(DEPLOY)/buildinfo
	cp build/flaginfo $(DEPLOY)/flaginfo
	cp build/gccinfo $(DEPLOY)/gccinfo
	chmod 0500 $(DEPLOY)/$(TARGET)

#install init script to /etc/init.d/	
	cp ../conf/etc/init.d/opendatalogger /etc/init.d/opendatalogger
	update-rc.d opendatalogger defaults
	ls -l /etc/rc?.d/*opendatalogger
	
#remove build
	#rm -r -f build
	
	@echo -e "$(OK_COLOR)[OK]$(NO_COLOR)"
	@echo "start the service by          sudo /etc/init.d/opendatalogger start"


uninstall: 
ifneq ($(user),root)
	@echo -e "$(ERROR_COLOR)make sure you have root privilegs prior uninstalling$(NO_COLOR)"
	@exit -1
endif
	@echo "uninstalling..."
	/etc/init.d/opendatalogger stop	
	
#remove /opt/opendatalogger
	rm -r $(DEPLOY)
	
#remove init script	
	rm -f /etc/init.d/opendatalogger
	update-rc.d -f opendatalogger remove
	
.PHONY: all debug gcov clean install uninstall
	
