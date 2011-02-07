DAEMON_NAME = des-aodv
VERSION_MAJOR = 1
VERSION_MINOR = 4
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)
DESTDIR ?=

DIR_BIN=$(DESTDIR)/usr/sbin
DIR_ETC=$(DESTDIR)/etc
DIR_ETC_DEF=$(DIR_ETC)/default
DIR_ETC_INITD=$(DIR_ETC)/init.d
TARFILES = src etc Makefile *.mk ChangeLog

CONFIG+=debug

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include src/pipeline/subdir.mk
-include src/subdir.mk
-include src/database/subdir.mk
-include src/database/schedule_table/subdir.mk
-include src/database/routing_table/subdir.mk
-include src/database/rl_seq_t/subdir.mk
-include src/database/packet_buffer/subdir.mk
-include src/database/neighbor_table/subdir.mk
-include src/database/iface_table/subdir.mk
-include src/database/broadcast_table/subdir.mk
-include src/database/rreq_log/subdir.mk
-include src/database/rerr_log/subdir.mk
-include src/cli/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

all: build

build: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -O0 -o $(DAEMON_NAME) $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

install:
	mkdir -p $(DIR_BIN)
	install -m 755 $(DAEMON_NAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 666 etc/$(DAEMON_NAME).conf $(DIR_ETC)
	mkdir -p $(DIR_ETC_DEF)
	install -m 644 etc/$(DAEMON_NAME).default $(DIR_ETC_DEF)/$(DAEMON_NAME)
	mkdir -p $(DIR_ETC_INITD)
	install -m 755 etc/$(DAEMON_NAME).init $(DIR_ETC_INITD)/$(DAEMON_NAME)

clean:
	-$(RM) $(OBJS)$(EXECUTABLES)$(C_DEPS) $(DAEMON_NAME) $(DAEMON_NAME)-$(VERSION).tar.gz $(DAEMON_NAME)-$(VERSION)
	-@echo ' '

tarball: clean
	mkdir $(DAEMON_NAME)-$(VERSION)
	cp -R $(TARFILES) $(DAEMON_NAME)-$(VERSION)
	find $(DAEMON_NAME)-$(VERSION) -name .svn -exec rm -rf {} +
	tar -czf $(DAEMON_NAME)-$(VERSION).tar.gz $(DAEMON_NAME)-$(VERSION)
	rm -rf $(DAEMON_NAME)-$(VERSION)

debian: tarball
	cp $(DAEMON_NAME)-$(VERSION).tar.gz ../debian/tarballs/$(DAEMON_NAME)_$(VERSION).orig.tar.gz
