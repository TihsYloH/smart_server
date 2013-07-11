.PHONY: arm x86 all clean install

arm:
	make clean
	export TARGET=arm && make all install

linux:
	make clean
	export TARGET=linux && make all install



LIB_INC := ../include
LIB_DIR := ../lib/$(TARGET)
INS_DIR := /home/ko/target
INCLUDE = -I$(LIB_INC)

CFLAGS = -O0 -g -Wall


DEMO_DISP=linux_server
DEMO_SERVER=data_server


ifeq ($(TARGET), arm)
CFLAGS += -static
LDFLAGS += -lm -lz -lstdc++
CC=arm-linux-gcc

#XML2_INC = ../include/libxml2 #XML2_INC = /usr/local/arm_linux_4.2/arm-linux/include/libxml2
                              #XML2_LIB = /usr/local/arm_linux_4.2/arm-linux/lib
#INCLUDE += -I$(XML2_INC)
#LDFLAGS += -L$(XML2_LIB) -lxml2 -lpthread -lm -lz -lstdc++
LDFLAGS += -lxml2 -lpthread -lm -lgdbm     #LDFLAGS += -L$(XML2_LIB) -lxml2 -lpthread -lm

#####################################################################
all: makedir $(DEMO_SERVER)
endif

ifeq ($(TARGET), linux)
					# linux SDL lib
CC=gcc

all:makedir $(DEMO_DISP)
endif



makedir:
	install -d $(INS_DIR)

clean:
	-rm $(DEMO_SERVER) $(DEMO_DISP)  *.o *.d *.exe -rf



$(DEMO_DISP): tcp_server.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@  intelligent.c tcp_server.c serial.c  $(LDFLAGS)

$(DEMO_SERVER): tcp_server.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@  intelligent.c tcp_server.c serial.c $(LDFLAGS)
	arm-linux-strip stream_server




install:
#	-cp $(DEMO_SAVEFILE) $(INS_DIR)
ifeq ($(TARGET), arm)
	-cp $(DEMO_SERVER) 	$(INS_DIR)
endif
ifeq ($(TARGET), linux)
	-cp $(DEMO_DISP) $(INS_DIR)
endif
#	-cp $(DEMO_DECODE) $(INS_DIR)
#	-cp $(DEMO_DISP) $(INS_DIR)
#	-cp $(DEMO_ASFREAD) $(INS_DIR)
#	-cp $(DEMO_ASFDISP) $(INS_DIR)
