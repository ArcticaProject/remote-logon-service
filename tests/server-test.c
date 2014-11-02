#include <glib.h>

#include "defines.h"
#include "server.h"
#include "citrix-server.h"
#include "rdp-server.h"
#include "uccs-server.h"

static gboolean
no_fatal_warnings (const gchar * log_domain, GLogLevelFlags level, const gchar * message, gpointer userdata)
{
	if (level & (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)) {
		return FALSE;
	}

	return TRUE;
}

static void
state_signal (Server * server, ServerState newstate, gboolean * signaled)
{
	*signaled = TRUE;
	return;
}

static void
test_update_signal (void)
{
	g_test_log_set_fatal_handler(no_fatal_warnings, NULL);

	GKeyFile * keyfile = g_key_file_new();
	const gchar * groupname = CONFIG_SERVER_PREFIX " Server Name";
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_NAME, "My Server");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_URI,  "http://my.domain.com");
	g_key_file_set_string(keyfile, groupname, CONFIG_UCCS_NETWORK, CONFIG_UCCS_NETWORK_NONE);

	Server * server = NULL;
	server = server_new_from_keyfile(keyfile, groupname);
	g_assert(server != NULL);
	g_assert(g_strcmp0(server->name, "My Server") == 0);
	g_assert(g_strcmp0(server->uri, "http://my.domain.com") == 0);

	UccsServer * userver = UCCS_SERVER(server);
	g_assert(userver != NULL);

	gboolean signaled = FALSE;
	g_signal_connect(G_OBJECT(server), SERVER_SIGNAL_STATE_CHANGED, G_CALLBACK(state_signal), &signaled);

	if (server->state == SERVER_STATE_ALLGOOD) {
		signaled = FALSE;
		uccs_server_set_exec(userver, "thisshouldnotexist");
		g_assert(signaled);
		g_assert(server->state == SERVER_STATE_UNAVAILABLE);
	}

	signaled = FALSE;
	uccs_server_set_exec(userver, "ls");
	g_assert(signaled);
	g_assert(server->state == SERVER_STATE_ALLGOOD);

	return;
}

static void
test_uccs_domains (void)
{
	g_test_log_set_fatal_handler(no_fatal_warnings, NULL);

	GKeyFile * keyfile = g_key_file_new();
	const gchar * groupname = CONFIG_SERVER_PREFIX " Server Name";
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_NAME, "My Server");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_URI,  "http://my.domain.com");

	Server * server = NULL;
	server = server_new_from_keyfile(keyfile, groupname);
	g_assert(server != NULL);
	g_assert(g_strcmp0(server->name, "My Server") == 0);
	g_assert(g_strcmp0(server->uri, "http://my.domain.com") == 0);

	GVariant * domains = server_cached_domains(server);
	g_assert(domains != NULL);
	g_assert(g_variant_is_of_type(domains, G_VARIANT_TYPE_ARRAY));
	g_variant_ref_sink(domains);
	g_variant_unref(domains);

	g_object_unref(server);
	g_key_file_unref(keyfile);

	return;
}

static void
test_uccs_exec (void)
{
	g_test_log_set_fatal_handler(no_fatal_warnings, NULL);

	GKeyFile * keyfile = g_key_file_new();
	const gchar * groupname = CONFIG_SERVER_PREFIX " Server Name";
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_NAME, "My Server");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_URI,  "http://my.domain.com");
	g_key_file_set_string(keyfile, groupname, CONFIG_UCCS_EXEC,  "ls");

	Server * server = NULL;
	server = server_new_from_keyfile(keyfile, groupname);
	g_assert(server != NULL);
	g_assert(g_strcmp0(server->name, "My Server") == 0);
	g_assert(g_strcmp0(server->uri, "http://my.domain.com") == 0);
	g_assert(g_strcmp0(UCCS_SERVER(server)->exec, "/bin/ls") == 0);

	g_object_unref(server);
	g_key_file_unref(keyfile);

	return;
}

typedef struct _type_data_t type_data_t;
struct _type_data_t {
	GType type;
	const gchar * name;
	const gchar * config_type;
};

type_data_t type_data[3] = {
	{0, "ica", CONFIG_SERVER_TYPE_ICA},
	{0, "freerdp", CONFIG_SERVER_TYPE_RDP},
	{0, "uccs", CONFIG_SERVER_TYPE_UCCS}
};

/*
static void
type_data_dump (type_data_t * data)
{
	g_debug("Type data: %X", data);
	g_debug("     type: %d", data->type);
	g_debug("     name: %s", data->name);
	g_debug("  c. type: %s", data->config_type);
}
*/

static gboolean
fatal_func (const gchar * log_domain, GLogLevelFlags level, const gchar * message, gpointer userdata)
{
	return FALSE;
}

static void
test_object_variant (gconstpointer data)
{
	g_test_log_set_fatal_handler(fatal_func, NULL);

	type_data_t * typedata = (type_data_t *)data;

	Server * server = g_object_new(typedata->type, NULL);

	g_assert(server != NULL);
	g_assert(IS_SERVER(server));

	server->name = g_strdup("My Name");
	server->uri = g_strdup("http://mysite.loves.testing.com");

	GVariant * variant = server_get_variant(server);

	g_assert(g_variant_n_children(variant) == 6);

	GVariant * child = g_variant_get_child_value(variant, 0);
	g_assert(g_variant_is_of_type(child, G_VARIANT_TYPE_STRING));
	g_assert(g_strcmp0(g_variant_get_string(child, NULL), typedata->name) == 0);
	g_variant_unref(child);

	child = g_variant_get_child_value(variant, 1);
	g_assert(g_variant_is_of_type(child, G_VARIANT_TYPE_STRING));
	g_assert(g_strcmp0(g_variant_get_string(child, NULL), "My Name") == 0);
	g_variant_unref(child);

	child = g_variant_get_child_value(variant, 2);
	g_assert(g_variant_is_of_type(child, G_VARIANT_TYPE_STRING));
	g_assert(g_strcmp0(g_variant_get_string(child, NULL), "http://mysite.loves.testing.com") == 0);
	g_variant_unref(child);

	g_variant_ref_sink(variant);
	g_variant_unref(variant);
	g_object_unref(G_OBJECT(server));

	return;
}

static void
test_object_keyfile_basics (gconstpointer data)
{
	g_test_log_set_fatal_handler(fatal_func, NULL);

	type_data_t * typedata = (type_data_t *)data;

	Server * server = NULL;

	server = server_new_from_keyfile(NULL, NULL);
	g_assert(server == NULL);

	GKeyFile * keyfile = g_key_file_new();
	server = server_new_from_keyfile(keyfile, NULL);
	g_assert(server == NULL);

	server = server_new_from_keyfile(keyfile, CONFIG_SERVER_PREFIX " Server Name");
	g_assert(server == NULL);

	gchar * groupname = g_strdup_printf("%s %s", CONFIG_SERVER_PREFIX, "Server Name");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_NAME, "My Server");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_URI,  "http://my.domain.com");
	g_key_file_set_string(keyfile, groupname, CONFIG_SERVER_TYPE, typedata->config_type);
	g_key_file_set_string(keyfile, groupname, "dumbledorf",  "Bad data, don't screw up!");

	server = server_new_from_keyfile(keyfile, CONFIG_SERVER_PREFIX " Server Name");
	g_assert(server != NULL);
	g_assert(g_strcmp0(server->name, "My Server") == 0);
	g_assert(g_strcmp0(server->uri, "http://my.domain.com") == 0);
	g_assert(typedata->type == G_OBJECT_TYPE(server));

	g_object_unref(server);
	g_free(groupname);
	g_key_file_unref(keyfile);

	return;
}

static void
test_object_creation (gconstpointer data)
{
	g_test_log_set_fatal_handler(no_fatal_warnings, NULL);

	type_data_t * typedata = (type_data_t *)data;

	Server * server = g_object_new(typedata->type, NULL);

	g_assert(server != NULL);
	g_assert(IS_SERVER(server));

	g_object_unref(G_OBJECT(server));
	return;
}

/* Build the test suite */
static void
test_objects_suite (void)
{
	/* Here because they might require code to create the type so can't be
	   statically defined in the global */
	type_data[0].type = CITRIX_SERVER_TYPE;
	type_data[1].type = RDP_SERVER_TYPE;
	type_data[2].type = UCCS_SERVER_TYPE;

	g_test_add_data_func ("/server/object/creation/citrix",  &type_data[0], test_object_creation);
	g_test_add_data_func ("/server/object/creation/rdp",     &type_data[1], test_object_creation);
	g_test_add_data_func ("/server/object/creation/uccs",    &type_data[2], test_object_creation);

	g_test_add_data_func ("/server/object/keyfile/citrix",  &(type_data[0]), test_object_keyfile_basics);
	g_test_add_data_func ("/server/object/keyfile/rdp",     &(type_data[1]), test_object_keyfile_basics);
	g_test_add_data_func ("/server/object/keyfile/uccs",    &(type_data[2]), test_object_keyfile_basics);

	g_test_add_data_func ("/server/object/variant/citrix",  &(type_data[0]), test_object_variant);
	g_test_add_data_func ("/server/object/variant/rdp",     &(type_data[1]), test_object_variant);
	g_test_add_data_func ("/server/object/variant/uccs",    &(type_data[2]), test_object_variant);

	g_test_add_func ("/server/uccs/exec",     test_uccs_exec);
	g_test_add_func ("/server/uccs/domains",  test_uccs_domains);
	g_test_add_func ("/server/uccs/signal",   test_update_signal);

	return;
}

gint
main (gint argc, gchar * argv[])
{
#if !GLIB_CHECK_VERSION (2, 35, 1)
	g_type_init();
#endif
	g_test_init(&argc, &argv, NULL);

	/* Test suites */
	test_objects_suite();

	return g_test_run ();
}
