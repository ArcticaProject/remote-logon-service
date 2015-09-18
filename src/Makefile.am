pkglibexec_PROGRAMS = \
	remote-logon-service

EXTRA_DIST = \
	org.ArcticaProject.RemoteLogon.xml

noinst_LTLIBRARIES = \
	libservers.la \
	libgenerated.la

################################
# libgenerated
################################

libgenerated_la_SOURCES = \
	remote-logon.c \
	remote-logon.h
libgenerated_la_CFLAGS = \
	$(SERVICE_CFLAGS)
$(libgenerated_la_SOURCES): org.ArcticaProject.RemoteLogon.xml
	gdbus-codegen \
		--interface-prefix org.ArcticaProject \
		--generate-c remote-logon \
		$^

BUILT_SOURCES = $(libgenerated_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

################################
# libservers
################################

libservers_la_SOURCES = \
	citrix-server.c \
	citrix-server.h \
	rdp-server.c \
	rdp-server.h \
	x2go-server.c \
	x2go-server.h \
	uccs-server.c \
	uccs-server.h \
	defines.h \
	server.c \
	server.h \
	crypt.c \
	crypt.h
libservers_la_CFLAGS = \
	-DUCCS_QUERY_TOOL="\"thin-client-config-agent\"" \
	$(SERVICE_CFLAGS) \
	$(COVERAGE_CFLAGS)
libservers_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS) $(GCRYPT_LIBS) -lm

################################
# remote-logon-service
################################

remote_logon_service_CFLAGS = \
	-DDEFAULT_CONFIG_FILE="\"$(sysconfdir)/remote-logon-service.conf\"" \
	$(SERVICE_CFLAGS) \
	$(COVERAGE_CFLAGS)
remote_logon_service_LDADD = \
	$(builddir)/libservers.la \
	$(builddir)/libgenerated.la \
	$(SERVICE_LIBS)
remote_logon_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)
remote_logon_service_SOURCES = \
	main.c
