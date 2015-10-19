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
CC:=gcc
CCFLAGS:=-std=gnu99 -O3
WFLAGS:=-Wall -Wfatal-errors -Wunused
LDFLAGS:=-lrt -lnetcdf -lm -lpthread
OBJECTS:=main_2.c i2c.c inifile.c filelock.c iniparser.c dictionary.c

user=$(shell whoami)
ifeq ($(user),root)
 targets := root
endif

	
help:
	@echo "usage:"
	@echo "        make clean           # removes output dir: build/ and debug/"
	@echo "        make all             # binary executable build/$(TARGET) will be created"
	@echo "        make install         # copies build to /opt/opendatalogger and "
	@echo "                                performs configuration (crontab) + setups"
	@echo "        make uninstall       # removes the installation"	
	@echo "        make debug           # gdb conform executable will be created for debugging"

clean: 
	@rm -r -f build
	@rm -r -f debug
	@rm -r -f log
	@echo "clean done"
		
all: $(OBJECTS)
	@rm -r -f debug		
	@mkdir -p build
# save gcc version information to file gccinfo
	@$(CC) -v 2> build/gccinfo
# save compiler flags into file flaginfo
	@echo "$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^" > build/flaginfo
# compile (and save output to file buildinfo
	$(CC) $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o build/$(TARGET) $^ 2>&1 | tee build/buildinfo
	@echo "all done"
	
debug: $(OBJECTS)
	@rm -r -f build
	@mkdir -p debug
	@$(CC) -E $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) $^ > debug/ads.obj
	$(CC) -g $(CFLAGS) $(PREPROC) $(LDFLAGS) $(WFLAGS) -o debug/$(TARGET) $^
	@echo "ready for debugging. use gdb"
	
install: ads
	@echo "beginning installing"
ifneq ($(targets),root)
	@echo "make sure you have root privilegs prior installing"
	@echo "will now exit"
	@exit 1
else
	@echo "installing..."
	@echo "not yet implemented"
endif
	@echo "install done"	

uninstall: ads
	@echo "beginning uninstalling"
ifneq ($(targets),root)
	@echo "make sure you have root privilegs prior uninstalling"
	@echo "will now exit"
	@exit 1
else
	@echo "uninstalling..."
	@echo "not yet implemented"
endif
	@echo "install done"	
	
.PHONY: all debug clean install uninstall
	
