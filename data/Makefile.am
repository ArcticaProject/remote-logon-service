
config_file_DATA = \
	remote-login-service.conf
config_filedir = $(sysconfdir)

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = com.canonical.RemoteLogin.service

%.service: %.service.in
	sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

%.conf: %.conf.in
	$(INTLTOOL_V_MERGE)LC_ALL=C $(INTLTOOL_MERGE) $(INTLTOOL_V_MERGE_OPTIONS) -k -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

EXTRA_DIST = \
	com.canonical.RemoteLogin.service.in \
	remote-login-service.conf.in

CLEANFILES = \
	com.canonical.RemoteLogin.service \
	remote-login-service.conf

