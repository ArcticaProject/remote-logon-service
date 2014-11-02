
#include <glib.h>
#include <gio/gio.h>
#include <libdbustest/dbus-test.h>

typedef struct _slmock_table_t slmock_table_t;
typedef struct _slmock_server_t slmock_server_t;

struct _slmock_table_t {
	const gchar * username;
	const gchar * password;
	slmock_server_t * servers;
};

struct _slmock_server_t {
	const gchar * name;
	const gchar * uri;
	const gchar * type;
	gboolean last_used;
	const gchar * username;
	const gchar * password;
	const gchar * domain;
};

slmock_server_t citrix_server_table[] = {
	{"Citrix USA", "107.21.17.35", "ica", FALSE, "useradmin1", "", "IP-0A00001E"},
	{"Citrix 2",   "107.21.17.35", "ica", FALSE, "useradmin2", "", "IP-0A00001E"},
	{"Citrix 3",   "107.21.17.35", "ica", FALSE, "useradmin3", "", "IP-0A00001E"},
	{"Citrix 4",   "107.21.17.35", "ica", FALSE, "useradmin4", "userpass", "IP-0A00001E"},
	{NULL, NULL, NULL}
};

slmock_server_t freerdp_server_table[] = {
	{"FreeRDP US",    "23.21.151.133",  "freerdp", TRUE, "Administrator", "", ""},
	{"FreeRDP Asia",  "46.137.222.181", "freerdp", FALSE, "Administrator", "", ""},
	{"FreeRDP UK",    "46.137.189.194", "freerdp", FALSE, "Administrator", "", ""},
	{NULL, NULL, NULL}
};

slmock_server_t freerdp_server_table_after_set_last_used[] = {
	{"FreeRDP US",    "23.21.151.133",  "freerdp", FALSE, "Administrator", "", ""},
	{"FreeRDP Asia",  "46.137.222.181", "freerdp", TRUE, "Administrator", "", ""},
	{"FreeRDP UK",    "46.137.189.194", "freerdp", FALSE, "Administrator", "", ""},
	{NULL, NULL, NULL}
};

slmock_server_t big_server_table[] = {
	{"XenServer",    "107.21.17.35",    "ica",     FALSE, "",         "",             "ASIA"},
	{"Citrix2",      "http://1.2.3.4",  "ica",     FALSE, "fakeuser", "fakepassword", "DOMAIN1"},
	{"Accenture",    "10.21.17.35",     "freerdp", FALSE, "fakeuser", "",             "EUROPE"},
	{"Accenture 2",  "https://4.5.6.7", "freerdp", TRUE, "",         "",             "domain2"},
	{NULL, NULL, NULL}
};

slmock_table_t slmock_table[] = {
	{"c", "c", citrix_server_table},
	{"f", "f", freerdp_server_table},
	{"b", "b", big_server_table},
	{"f", "f", freerdp_server_table_after_set_last_used}
};

static gboolean
find_server (GVariant * varray, slmock_server_t * server)
{
	int ichild;
	for (ichild = 0; ichild < g_variant_n_children(varray); ichild++) {
		GVariant * child = g_variant_get_child_value(varray, ichild);
		gboolean test = FALSE;

		GVariant * type = g_variant_get_child_value(child, 0);
		test = g_strcmp0(g_variant_get_string(type, NULL), server->type) == 0;
		g_variant_unref(type);
		if (!test) {
			g_variant_unref(child);
			continue;
		}

		GVariant * name = g_variant_get_child_value(child, 1);
		test = g_strcmp0(g_variant_get_string(name, NULL), server->name) == 0;
		g_variant_unref(name);
		if (!test) {
			g_variant_unref(child);
			continue;
		}

		GVariant * uri = g_variant_get_child_value(child, 2);
		test = g_strcmp0(g_variant_get_string(uri, NULL), server->uri) == 0;
		g_variant_unref(uri);
		if (!test) {
			g_variant_unref(child);
			continue;
		}

		GVariant * last_used = g_variant_get_child_value(child, 3);
		test = g_variant_get_boolean(last_used) == server->last_used;
		g_variant_unref(last_used);
		if (!test) {
			g_variant_unref(child);
			continue;
		}

		gboolean match_username = FALSE;
		gboolean match_password = FALSE;
		gboolean match_domain = FALSE;

		GVariant * props = g_variant_get_child_value(child, 4);
		int iprop;
		for (iprop = 0; iprop < g_variant_n_children(props); iprop++) {
			GVariant * prop = g_variant_get_child_value(props, iprop);

			GVariant * prop_type = g_variant_get_child_value(prop, 0);
			GVariant * prop_value_wrap = g_variant_get_child_value(prop, 2);
			GVariant * prop_value = g_variant_get_variant(prop_value_wrap);

			if (g_strcmp0(g_variant_get_string(prop_type, NULL), "username") == 0) {
				if (g_strcmp0(g_variant_get_string(prop_value, NULL), server->username) == 0) {
					match_username = TRUE;
				}
			} else if (g_strcmp0(g_variant_get_string(prop_type, NULL), "password") == 0) {
				if (g_strcmp0(g_variant_get_string(prop_value, NULL), server->password) == 0) {
					match_password = TRUE;
				}
			} else if (g_strcmp0(g_variant_get_string(prop_type, NULL), "domain") == 0) {
				if (g_strcmp0(g_variant_get_string(prop_value, NULL), server->domain) == 0) {
					match_domain = TRUE;
				}
			}

			g_variant_unref(prop_value);
			g_variant_unref(prop_value_wrap);
			g_variant_unref(prop_type);
			g_variant_unref(prop);
		}
		g_variant_unref(props);

		g_variant_unref(child);

		if (match_username && match_password && match_domain) {
			return TRUE;
		} else {
			continue;
		}
	}

	return FALSE;
}

static gboolean
slmock_check_login(GDBusConnection * session, slmock_table_t * slmockdata, gboolean clear_cache)
{
	if (clear_cache) {
		gchar *username_sha = g_compute_checksum_for_string (G_CHECKSUM_SHA256, slmockdata->username, -1);
		gchar *file_path = g_build_path ("/", g_get_user_cache_dir(), "remote-login-service", "cache", username_sha, NULL);
		unlink (file_path);
		g_free (username_sha);
		g_free (file_path);
	}
	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "GetServersForLogin",
	                                                g_variant_new("(sssb)",
	                                                              "https://slmock.com/",
	                                                              slmockdata->username,
	                                                              slmockdata->password,
	                                                              TRUE), /* params */
	                                                G_VARIANT_TYPE("(bsa(sssba(sbva{sv})a(si)))"), /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                NULL);

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 3);

	GVariant * loggedin = g_variant_get_child_value(retval, 0);
	g_assert(g_variant_get_boolean(loggedin));
	g_variant_unref(loggedin); loggedin = NULL;

	GVariant * data = g_variant_get_child_value(retval, 1);
	g_assert(g_strcmp0(g_variant_get_string(data, NULL), "network") == 0);
	g_variant_unref(data); data = NULL;

	GVariant * array = g_variant_get_child_value(retval, 2);
	int i;
	for (i = 0; slmockdata->servers[i].name != NULL; i++) {
		g_assert(find_server(array, &slmockdata->servers[i]));
	}
	g_variant_unref(array);

	g_variant_unref(retval);

	return TRUE;
}

static void
test_getservers_slmock (gconstpointer data)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" SLMOCK_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	g_assert(slmock_check_login(session, (slmock_table_t *)data, TRUE));

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}

static void
test_getservers_slmock_none (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" SLMOCK_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "GetServersForLogin",
	                                                g_variant_new("(sssb)",
	                                                              "https://slmock.com/",
	                                                              "Baduser",
	                                                              "Badpass",
	                                                              TRUE), /* params */
	                                                G_VARIANT_TYPE("(bsa(sssba(sbva{sv})a(si)))"), /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                NULL);

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 3);

	GVariant * loggedin = g_variant_get_child_value(retval, 0);
	g_assert(!g_variant_get_boolean(loggedin));
	g_variant_unref(loggedin); loggedin = NULL;

	GVariant * array = g_variant_get_child_value(retval, 2);
	g_assert(g_variant_n_children(array) == 0);
	g_variant_unref(array);
	g_variant_unref(retval);

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}

static void
test_getservers_slmock_two (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" SLMOCK_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	g_assert(slmock_check_login(session, &slmock_table[0], TRUE));
	g_assert(slmock_check_login(session, &slmock_table[1], TRUE));

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}

static void
test_getservers_none (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" NULL_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "GetServers",
	                                                NULL, /* params */
	                                                G_VARIANT_TYPE("(a(sssba(sbva{sv})a(si)))"), /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                NULL);

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 1);

	GVariant * array = g_variant_get_child_value(retval, 0);
	g_assert(g_variant_n_children(array) == 0);
	g_assert(g_variant_is_of_type(array, G_VARIANT_TYPE_ARRAY));

	g_variant_unref(array);
	g_variant_unref(retval);

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}

static void
test_getservers_uccs (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" UCCS_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "GetServers",
	                                                NULL, /* params */
	                                                G_VARIANT_TYPE("(a(sssba(sbva{sv})a(si)))"), /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                NULL);

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 1);

	GVariant * array = g_variant_get_child_value(retval, 0);
	g_assert(g_variant_n_children(array) == 1);
	g_assert(g_variant_is_of_type(array, G_VARIANT_TYPE_ARRAY));

	GVariant * tuple = g_variant_get_child_value(array, 0);

	if (TRUE) { /* type check */
		GVariant * type = g_variant_get_child_value(tuple, 0);
		g_assert(g_variant_is_of_type(type, G_VARIANT_TYPE_STRING));
		g_assert(g_strcmp0(g_variant_get_string(type, NULL), "uccs") == 0);
		g_variant_unref(type);
	}

	if (TRUE) { /* name check */
		GVariant * name = g_variant_get_child_value(tuple, 1);
		g_assert(g_variant_is_of_type(name, G_VARIANT_TYPE_STRING));
		g_assert(g_strcmp0(g_variant_get_string(name, NULL), "Test Server Name") == 0);
		g_variant_unref(name);
	}

	if (TRUE) { /* uri check */
		GVariant * uri = g_variant_get_child_value(tuple, 2);
		g_assert(g_variant_is_of_type(uri, G_VARIANT_TYPE_STRING));
		g_assert(g_strcmp0(g_variant_get_string(uri, NULL), "https://uccs.test.mycompany.com/") == 0);
		g_variant_unref(uri);
	}

	g_variant_unref(tuple);
	g_variant_unref(array);
	g_variant_unref(retval);

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}

static void
test_getdomains_basic (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" SLMOCK_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);

	GError * error = NULL;
	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "GetCachedDomainsForServer",
	                                                g_variant_new("(s)", "https://slmock.com/"), /* params */
	                                                G_VARIANT_TYPE("(as)"), /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                &error);

	if (error != NULL) {
		g_debug("Error from GetCachedDomainsForServer: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 1);

	GVariant * child = g_variant_get_child_value(retval, 0);
	g_assert(g_variant_is_of_type(child, G_VARIANT_TYPE_ARRAY));
	g_variant_unref(child);

	g_variant_unref(retval);

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);

	return;
}


static void
test_setlastused_basic (void)
{
	DbusTestService * service = dbus_test_service_new(NULL);

	/* RLS */
	DbusTestProcess * rls = dbus_test_process_new(REMOTE_LOGIN_SERVICE);
	dbus_test_process_append_param(rls, "--config-file=" SLMOCK_CONFIG_FILE);
	dbus_test_service_add_task(service, DBUS_TEST_TASK(rls));

	/* Dummy */
	DbusTestTask * dummy = dbus_test_task_new();
	dbus_test_task_set_wait_for(dummy, "com.canonical.RemoteLogin");
	dbus_test_service_add_task(service, dummy);

	/* Get RLS up and running and us on that bus */
	dbus_test_service_start_tasks(service);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	g_dbus_connection_set_exit_on_close(session, FALSE);
	
	g_assert(slmock_check_login(session, &slmock_table[1], TRUE));

	GError * error = NULL;
	GVariant * retval = g_dbus_connection_call_sync(session,
	                                                "com.canonical.RemoteLogin",
	                                                "/com/canonical/RemoteLogin",
	                                                "com.canonical.RemoteLogin",
	                                                "SetLastUsedServer",
	                                                g_variant_new("(ss)", "Landscape", freerdp_server_table[1].uri), /* params */
	                                                NULL, /* ret type */
	                                                G_DBUS_CALL_FLAGS_NONE,
	                                                -1,
	                                                NULL,
	                                                &error);

	if (error != NULL) {
		g_debug("Error from SetLastUsedServer: %s", error->message);
		g_error_free(error);
		error = NULL;
	}

	g_assert(retval != NULL);
	g_assert(g_variant_n_children(retval) == 0);
	g_variant_unref(retval);
	
	g_assert(slmock_check_login(session, &slmock_table[3], FALSE));

	g_object_unref(session);
	g_object_unref(rls);
	g_object_unref(service);
	
	return;
}


/* Build the test suite */
static void
test_dbus_suite (void)
{
	g_test_add_func ("/dbus/interface/GetServers/None",   test_getservers_none);
	g_test_add_func ("/dbus/interface/GetServers/UCCS",   test_getservers_uccs);
	g_test_add_data_func ("/dbus/interface/GetServers/SLMock/citrix",  &slmock_table[0], test_getservers_slmock);
	g_test_add_data_func ("/dbus/interface/GetServers/SLMock/freerdp", &slmock_table[1], test_getservers_slmock);
	g_test_add_data_func ("/dbus/interface/GetServers/SLMock/big",     &slmock_table[2], test_getservers_slmock);
	g_test_add_func ("/dbus/interface/GetServers/SLMock/none",   test_getservers_slmock_none);
	g_test_add_func ("/dbus/interface/GetServers/SLMock/two",   test_getservers_slmock_two);
	g_test_add_func ("/dbus/interface/GetDomains/Basic",   test_getdomains_basic);
	g_test_add_func ("/dbus/interface/SetLastUsed/Basic",   test_setlastused_basic);

	return;
}

gint
main (gint argc, gchar * argv[])
{
	g_type_init();
	g_test_init(&argc, &argv, NULL);

	/* Test suites */
	test_dbus_suite();

	return g_test_run ();
}
