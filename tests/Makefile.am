NULL =

CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
check_PROGRAMS =

TESTS =											\
        server-tester									\
        dbus-interface-tester								\
        $(NULL)

#####################################
# Server Test
#####################################

SERVER_XML_REPORT = server-test.xml
server-tester: server-test Makefile.am
	@echo "#!/bin/bash" > $@
	@echo export DBUS_TEST_RUNNER=1 >> $@
	@echo gtester --verbose -k -o $(SERVER_XML_REPORT) $(abs_builddir)/server-test >> $@
	@chmod +x $@

CLEANFILES +=										\
        $(SERVER_XML_REPORT)								\
        server-tester									\
        $(NULL)

server_test_SOURCES =									\
        server-test.c									\
        $(NULL)

server_test_CFLAGS =									\
        -I$(top_srcdir)/src								\
        -I$(top_builddir)/src								\
        -Werror										\
        $(SERVICE_CFLAGS)								\
        $(NULL)

server_test_LDADD =									\
        $(top_builddir)/src/libservers.la						\
        $(SERVICE_LIBS)									\
        $(NULL)

check_PROGRAMS += server-test

#####################################
# DBus Interface
#####################################

DBUS_XML_REPORT = dbus-interface.xml
dbus-interface-tester: dbus-interface uccs-config.conf slmock-config.conf Makefile.am
	@echo "#!/bin/bash" > $@
	@echo gtester --verbose -k -o $(DBUS_XML_REPORT) $(abs_builddir)/dbus-interface >> $@
	@chmod +x $@

CLEANFILES +=										\
        $(DBUS_XML_REPORT)								\
        dbus-interface-tester								\
        slmock-config.conf								\
        $(NULL)

EXTRA_DIST +=										\
        null-config.conf								\
        uccs-config.conf								\
        slmock										\
        slmock-config.conf.in								\
        $(NULL)

slmock-config.conf: slmock-config.conf.in
	sed -e "s|\@slmock\@|$(abs_srcdir)/slmock|" $< > $@

dbus_interface_SOURCES =			\
        dbus-interface.c			\
        $(NULL)

dbus_interface_CFLAGS =									\
        -DREMOTE_LOGON_SERVICE="\"$(abs_top_builddir)/src/remote-logon-service\""	\
        -DUCCS_CONFIG_FILE="\"$(abs_srcdir)/uccs-config.conf\""				\
        -DSLMOCK_CONFIG_FILE="\"$(abs_builddir)/slmock-config.conf\""			\
        -DNULL_CONFIG_FILE="\"$(abs_srcdir)/null-config.conf\""				\
        -Werror										\
        $(SERVICE_CFLAGS)								\
        $(TEST_CFLAGS)									\
        $(NULL)

dbus_interface_LDADD =									\
        $(SERVICE_LIBS)									\
        $(TEST_LIBS)									\
        $(NULL)

check_PROGRAMS += dbus-interface
