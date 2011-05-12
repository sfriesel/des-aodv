DAEMONNAME = des-aodv
DESTDIR ?= /usr/sbin
PREFIX ?=
NAME = "aodv"

DIR_BIN = $(PREFIX)$(DESTDIR)
DIR_ETC = $(PREFIX)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d
MODULES = src/aodv src/helper src/cli/aodv_cli src/database/aodv_database src/database/timeslot src/database/broadcast_table/aodv_broadcast_t src/database/neighbor_table/nt \
	src/database/packet_buffer/packet_buffer src/database/rerr_log/rerr_log src/database/rl_seq_t/rl_seq src/database/routing_table/aodv_rt src/database/rreq_log/rreq_log \
	src/database/schedule_table/aodv_st src/pipeline/aodv_periodic src/pipeline/aodv_pipeline

UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = *.c *.h Makefile *.conf *.init *.default *.sh build version major_version ChangeLog *.lua

FILE_DEFAULT = etc/$(DAEMONNAME).default
FILE_ETC = etc/$(DAEMONNAME).conf
FILE_INIT = etc/$(DAEMONNAME).init

LIBS = dessert dessert-extra pthread cli
CFLAGS += -ggdb -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE -I/usr/include -I/usr/local/include
LDFLAGS += $(addprefix -l,$(LIBS))

all: build

clean:
	rm -f *.o *.tar.gz ||  true
	find . -name *.o -delete
	rm -f $(DAEMONNAME) || true
	rm -rf $(DAEMONNAME).dSYM || true

install:
	mkdir -p $(DIR_BIN)
	install -m 744 $(DAEMONNAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 644 $(FILE_ETC) $(DIR_ETC)
	mkdir -p $(DIR_DEFAULT)
	install -m 644 $(FILE_DEFAULT) $(DIR_DEFAULT)/$(DAEMONNAME)
	mkdir -p $(DIR_INIT)
	install -m 755 $(FILE_INIT) $(DIR_INIT)/$(DAEMONNAME)

build: $(addsuffix .o,$(MODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))

android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert -ldessert-extra
android: build package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir des-aodv
	cp -r $(TARFILES) des-aodv
	tar -czf des-aodv.tar.gz des-aodv
	rm -rf des-aodv

debian: tarball
	cp $(DAEMONNAME).tar.gz ../debian/tarballs/$(DAEMONNAME).orig.tar.gz

.SILENT: clean
