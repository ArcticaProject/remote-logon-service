/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ted Gould <ted@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

/* NOTE: Required to build without optimizations */
#include <locale.h>

#include "remote-login.h"
#include "defines.h"

#include "server.h"
#include "rdp-server.h"
#include "citrix-server.h"
#include "uccs-server.h"
#include "x2go-server.h"
#include "crypt.h"

gint server_list_to_array (GVariantBuilder * builder, GList * items);

enum {
	ERROR_SERVER_URI,
	ERROR_LOGIN
};

GList * config_file_servers = NULL;

/* Get the error domain for this module */
static GQuark
error_domain (void)
{
	static GQuark value = 0;
	if (value == 0) {
		value = g_quark_from_static_string("remote-login-service");
	}
	return value;
}

/* When one of the state changes on the server emit that so that everone knows there
   might be a new server available. */
static void
server_status_updated (Server * server, ServerState newstate, RemoteLogin * rl)
{
	GVariant * array = NULL;

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

	if (server_list_to_array(&builder, config_file_servers) > 0) {
		array = g_variant_builder_end(&builder);
	} else {
		g_variant_builder_clear(&builder);
		array = g_variant_new_array(G_VARIANT_TYPE("(sssba(sbva{sv})a(si))"), NULL, 0);
	}

	remote_login_emit_servers_updated(rl, array);
	return;
}

/* Looks for the config file and does some basic parsing to pull out the UCCS servers
   that are configured in it */
static void
find_config_file (GKeyFile * parsed, const gchar * cmnd_line, RemoteLogin * rl)
{
	GError * error = NULL;
	const gchar * file = DEFAULT_CONFIG_FILE;

	if (cmnd_line != NULL) {
		file = cmnd_line;
	}

	if (!g_key_file_load_from_file(parsed, file, G_KEY_FILE_NONE, &error)) {
		g_warning("Unable to parse config file '%s': %s", file, error->message);
		g_error_free(error);
		return;
	}

	if (!g_key_file_has_group(parsed, CONFIG_MAIN_GROUP)) {
		g_warning("Config file '%s' doesn't have group '" CONFIG_MAIN_GROUP "'", file);
		/* Probably should clear the keyfile, but there doesn't seem to be a way to do that */
		return;
	}

	if (g_key_file_has_key(parsed, CONFIG_MAIN_GROUP, CONFIG_MAIN_SERVERS, NULL)) {
		gchar ** grouplist = g_key_file_get_string_list(parsed, CONFIG_MAIN_GROUP, CONFIG_MAIN_SERVERS, NULL, NULL);
		int i = 0;
		gchar * groupsuffix;

		for (groupsuffix = grouplist[0], i = 0; groupsuffix != NULL; groupsuffix = grouplist[++i]) {
			gchar * groupname = g_strdup_printf("%s %s", CONFIG_SERVER_PREFIX, groupsuffix);
			Server * server = server_new_from_keyfile(parsed, groupname);
			g_free(groupname);

			if (server == NULL) {
				/* Assume a relevant error is printed above */
				continue;
			}

			config_file_servers = g_list_append(config_file_servers, server);
			g_signal_connect(server, SERVER_SIGNAL_STATE_CHANGED, G_CALLBACK(server_status_updated), rl);
		}

		g_strfreev(grouplist);
	}

	/* Signal the list of servers so that we're sure everyone's got them.  This is to
	   solve a possible race where someone could ask while we're configuring these. */
	server_status_updated(NULL, SERVER_STATE_ALLGOOD, rl);
	return;
}

gint
server_list_to_array (GVariantBuilder * builder, GList * items)
{
	gint servercnt = 0;
	GList * head = NULL;
	for (head = items; head != NULL; head = g_list_next(head)) {
		Server * server = SERVER(head->data);

		/* We only want servers that are all good */
		if (server->state != SERVER_STATE_ALLGOOD) {
			continue;
		}

		servercnt++;
		GVariant * variant = server_get_variant(server);
		g_variant_builder_add_value(builder, variant);
	}

	return servercnt;
}

static gboolean
handle_get_servers (RemoteLogin * rl, GDBusMethodInvocation * invocation, gpointer user_data)
{
	GVariant * array = NULL;

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

	if (server_list_to_array(&builder, config_file_servers) > 0) {
		array = g_variant_builder_end(&builder);
	} else {
		g_variant_builder_clear(&builder);
		array = g_variant_new_array(G_VARIANT_TYPE("(sssba(sbva{sv})a(si))"), NULL, 0);
	}

	g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&array, 1));

	return TRUE;
}

/* Handle the situation of whether we unlock or not and respond over
   DBus with either an error or the list of servers.  */
static void
handle_get_servers_login_cb (UccsServer * server, gboolean unlocked, gpointer user_data)
{
	GDBusMethodInvocation * invocation = (GDBusMethodInvocation *)user_data;
	const gchar * sender = g_dbus_method_invocation_get_sender(invocation);

	GVariantBuilder builder;
	g_variant_builder_init(&builder, G_VARIANT_TYPE_TUPLE);

	/* Signal whether we're unlocked */
	g_variant_builder_add_value(&builder, g_variant_new_boolean(unlocked));

	/* Only network, no caching yet */
	g_variant_builder_add_value(&builder, g_variant_new_string("network"));

	/* Get the array of servers */
	GVariant * array = uccs_server_get_servers(server, sender);
	g_variant_builder_add_value(&builder, array);

	g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
	return;
}

/* Handle the GetServerForLogin DBus call */
static gboolean
handle_get_servers_login (RemoteLogin * rl, GDBusMethodInvocation * invocation, gpointer user_data)
{
	GVariant * params = g_dbus_method_invocation_get_parameters(invocation);
	const gchar * sender = g_dbus_method_invocation_get_sender(invocation);

	GVariant * child = NULL;
	const gchar * uri = NULL;

	child = g_variant_get_child_value(params, 0);
	uri = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */

	GList * lserver = NULL;
	Server * server = NULL;
	for (lserver = config_file_servers; lserver != NULL; lserver = g_list_next(lserver)) {
		server = SERVER(lserver->data);

		if (server == NULL) {
			continue;
		}

		if (!IS_UCCS_SERVER(server)) {
			continue;
		}

		if (g_strcmp0(server->uri, uri) == 0) {
			break;
		}
	}

	if (lserver == NULL) {
		/* Couldn't find something with that URI, we're done, thanks. */
		g_dbus_method_invocation_return_error(invocation,
		                                      error_domain(),
		                                      ERROR_SERVER_URI,
		                                      "Unable to find a server with the URI: '%s'",
		                                      uri);

		return TRUE;
	}

	/* Unlock the Server */
	const gchar * username = NULL;
	const gchar * password = NULL;
 	gboolean allowcache = FALSE;

	child = g_variant_get_child_value(params, 1);
	username = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */

	child = g_variant_get_child_value(params, 2);
	password = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */

 	child = g_variant_get_child_value(params, 3);
 	allowcache = g_variant_get_boolean(child);
 	g_variant_unref(child);

	/* Try to login and mark us as servicing the message */
	uccs_server_unlock(UCCS_SERVER(server), sender, username, password, allowcache, handle_get_servers_login_cb, invocation);
	return TRUE;
}

/* Look through a list of servers to see if one matches a URL */
static Server *
handle_get_domains_list_helper (GList * list, const gchar * uri)
{
	if (list == NULL) return NULL;

	Server * inserver = SERVER(list->data);

	if (inserver == NULL) {
		return handle_get_domains_list_helper(g_list_next(list), uri);
	}

	Server * outserver = server_find_uri(inserver, uri);

	if (outserver != NULL) {
		return outserver;
	}

	return handle_get_domains_list_helper(g_list_next(list), uri);
}

/* Get the cached domains for a server */
static gboolean
handle_get_domains (RemoteLogin * rl, GDBusMethodInvocation * invocation, gpointer user_data)
{
	GVariant * params = g_dbus_method_invocation_get_parameters(invocation);

	GVariant * child = NULL;
	const gchar * uri = NULL;

	child = g_variant_get_child_value(params, 0);
	uri = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */

	Server * server = handle_get_domains_list_helper(config_file_servers, uri);

	GVariant * domains = NULL;
	if (server != NULL) {
		domains = server_cached_domains(server);
	} else {
		domains = g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
	}

	if (domains == NULL) {
		/* Couldn't find something with that URI, we're done, thanks. */
		g_dbus_method_invocation_return_error(invocation,
		                                      error_domain(),
		                                      ERROR_SERVER_URI,
		                                      "Unable to find a server with the URI: '%s'",
		                                      uri);

		return TRUE;
	}

	g_dbus_method_invocation_return_value(invocation, g_variant_new_tuple(&domains, 1));

	return TRUE;
}

/* Set a given server as last used */
static gboolean
handle_set_last_used_server (RemoteLogin * rl, GDBusMethodInvocation * invocation, gpointer user_data)
{
	GVariant * params = g_dbus_method_invocation_get_parameters(invocation);

	GVariant * child = NULL;
	const gchar * uccsUri = NULL;
	const gchar * serverUri = NULL;

	child = g_variant_get_child_value(params, 0);
	uccsUri = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */
	
	child = g_variant_get_child_value(params, 1);
	serverUri = g_variant_get_string(child, NULL);
	g_variant_unref(child); /* fine as we know params is still ref'd */
	
	GList * lserver = NULL;
	Server * server = NULL;
	for (lserver = config_file_servers; lserver != NULL; lserver = g_list_next(lserver)) {
		server = SERVER(lserver->data);

		if (server == NULL) {
			continue;
		}

		if (!IS_UCCS_SERVER(server)) {
			continue;
		}

		if (g_strcmp0(server->uri, uccsUri) == 0) {
			break;
		}
	}
	
	if (server != NULL) {
		server_set_last_used_server (server, serverUri);
	}
	
	g_dbus_method_invocation_return_value(invocation, NULL);
	
	return TRUE;
}

/* If we loose the name, tell the world and there's not much we can do */
static void
name_lost (GDBusConnection * connection, const gchar * name, gpointer user_data)
{
	GMainLoop * mainloop = (GMainLoop *)user_data;

	g_warning("Unable to get name '%s'.  Exiting.", name);
	g_main_loop_quit(mainloop);

	return;
}

static gchar * cmnd_line_config = NULL;

static GOptionEntry general_options[] = {
	{"config-file",  'c',  0,  G_OPTION_ARG_FILENAME,  &cmnd_line_config, N_("Configuration file for the remote login service.  Defaults to '/etc/remote-login-service.conf'."), N_("key_file")},
	{NULL}
};

int
main (int argc, char * argv[])
{
	GError * error = NULL;

#if !GLIB_CHECK_VERSION (2, 35, 1)
	/* Init the GTypes */
	g_type_init();
#endif

	/* Setup i18n */
	setlocale (LC_ALL, ""); 
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	/* Create our global variables */
	GKeyFile * config = g_key_file_new();
	GMainLoop * mainloop = g_main_loop_new(NULL, FALSE);

	/* Handle command line parameters */
	GOptionContext * context;
	context = g_option_context_new(_("- Determine the remote servers that can be logged into"));
	g_option_context_add_main_entries(context, general_options, "remote-login-service");

	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		g_error_free(error);
		return 1;
	}

	/* Start up D' Bus */
	GDBusConnection * session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL /* cancel */, &error);
	if (error != NULL) {
		g_error("Unable to get session bus: %s", error->message);
		g_error_free(error);
		return -1;
	}

	/* Build Dbus Interface */
	RemoteLogin * skel = remote_login_skeleton_new();
	/* Export it */
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skel),
	                                 session_bus,
	                                 "/com/canonical/RemoteLogin",
	                                 NULL);
	g_signal_connect(skel, "handle-get-servers", G_CALLBACK(handle_get_servers), NULL);
	g_signal_connect(skel, "handle-get-servers-for-login", G_CALLBACK(handle_get_servers_login), NULL);
	g_signal_connect(skel, "handle-get-cached-domains-for-server", G_CALLBACK(handle_get_domains), NULL);
	g_signal_connect(skel, "handle-set-last-used-server", G_CALLBACK(handle_set_last_used_server), NULL);

	g_bus_own_name_on_connection(session_bus,
	                             "com.canonical.RemoteLogin",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, /* aquired handler */
	                             name_lost,
	                             mainloop,
	                             NULL); /* mainloop free */

	/* Parse config file */
	find_config_file(config, cmnd_line_config, skel);

	/* Loop forever */
	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);
	g_object_unref(config);

	g_free(cmnd_line_config);

	return 0;
}
