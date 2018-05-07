NULL =

config_file_DATA = remote-logon-service.conf
config_filedir = $(sysconfdir)

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = org.ArcticaProject.RemoteLogon.service

%.service: %.service.in
	sed -e "s|\@pkglibexecdir\@|$(pkglibexecdir)|" $< > $@

%.conf: %.conf.in
	$(INTLTOOL_V_MERGE)LC_ALL=C $(INTLTOOL_MERGE) $(INTLTOOL_V_MERGE_OPTIONS) -k -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

EXTRA_DIST =								\
        org.ArcticaProject.RemoteLogon.service.in			\
        remote-logon-service.conf.in					\
        $(NULL)

CLEANFILES = \
        org.ArcticaProject.RemoteLogon.service				\
        remote-logon-service.conf					\
        $(NULL)
