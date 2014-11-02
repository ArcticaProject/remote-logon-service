
pkglibexec_PROGRAMS = \
	remote-login-service

EXTRA_DIST = \
	com.canonical.RemoteLogin.xml

noinst_LTLIBRARIES = \
	libservers.la \
	libgenerated.la

################################
# libgenerated
################################

libgenerated_la_SOURCES = \
	remote-login.c \
	remote-login.h
libgenerated_la_CFLAGS = \
	$(SERVICE_CFLAGS)
$(libgenerated_la_SOURCES): com.canonical.RemoteLogin.xml
	gdbus-codegen \
		--interface-prefix com.canonical \
		--generate-c remote-login \
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
# remote-login-service
################################

remote_login_service_CFLAGS = \
	-DDEFAULT_CONFIG_FILE="\"$(sysconfdir)/remote-login-service.conf\"" \
	$(SERVICE_CFLAGS) \
	$(COVERAGE_CFLAGS)
remote_login_service_LDADD = \
	$(builddir)/libservers.la \
	$(builddir)/libgenerated.la \
	$(SERVICE_LIBS)
remote_login_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)
remote_login_service_SOURCES = \
	main.c

