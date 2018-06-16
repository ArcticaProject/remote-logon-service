/*
 * Copyright © 2012 Canonical Ltd.
 * Copyright © 2015 The Arctica Project
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
 * Authors: Ted Gould <ted@canonical.com>
 *          Mike Gabriel <mike.gabriel@das-netzwerkteam.de>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <json-glib/json-glib.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <stdlib.h>
#include <string.h>

#include "uccs-server.h"
#include "defines.h"

#include "rdp-server.h"
#include "citrix-server.h"

#include "crypt.h"

static void uccs_server_class_init (UccsServerClass *klass);
static void uccs_server_init       (UccsServer *self);
static void uccs_server_dispose    (GObject *object);
static void uccs_server_finalize   (GObject *object);
static GVariant *  get_properties        (Server * server);
static void json_waiters_notify (UccsServer * server, gboolean unlocked);
static GVariant * get_cached_domains (Server * server);
static Server * find_uri (Server * server, const gchar * uri);
static void set_last_used_server (Server * server, const gchar * uri);
static void nm_state_changed (NMClient *client, const GParamSpec *pspec, gpointer user_data);

typedef struct _json_callback_t json_callback_t;
struct _json_callback_t {
	gchar * sender;
	void (*callback) (UccsServer * server, gboolean unlocked, gpointer user_data);
	gpointer userdata;
};

G_DEFINE_TYPE (UccsServer, uccs_server, SERVER_TYPE);

/* Static global client so we don't keep reallocating them.  We only need
   one really */
static NMClient * global_client = NULL;

static void
uccs_server_class_init (UccsServerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = uccs_server_dispose;
	object_class->finalize = uccs_server_finalize;

	ServerClass * server_class = SERVER_CLASS(klass);

	server_class->get_properties = get_properties;
	/* UCCS can't have applications */
	server_class->get_applications = NULL;
	server_class->get_domains = get_cached_domains;
	server_class->find_uri = find_uri;
	server_class->set_last_used_server = set_last_used_server;

	return;
}

static void
uccs_server_init (UccsServer *self)
{
	self->exec = g_find_program_in_path(UCCS_QUERY_TOOL);

	self->username = NULL;
	self->password = NULL;

	self->lovers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	self->subservers = NULL;

	self->json_waiters = NULL;
	self->json_watch = 0;
	self->json_pid = 0;

	self->json_stream = NULL;
	self->pass_stream = NULL;

	self->min_network = NM_STATE_CONNECTED_GLOBAL;
	self->last_network = NM_STATE_DISCONNECTED;
	self->nm_client = NULL;
	self->nm_signal = 0;

	/* Start as unavailable */
	self->parent.state = SERVER_STATE_UNAVAILABLE;

	if (g_strcmp0(g_getenv("DBUS_TEST_RUNNER"), "1")) {

		if (global_client == NULL) {
			global_client = nm_client_new(NULL, NULL);

			if (global_client != NULL) {
				g_object_add_weak_pointer(G_OBJECT(global_client), (gpointer *)&global_client);
				self->nm_client = global_client;
			}
		} else {
			self->nm_client = g_object_ref(global_client);
		}

		if (self->nm_client != NULL) {
			self->nm_signal = g_signal_connect(self->nm_client, "notify::" NM_CLIENT_STATE, G_CALLBACK(nm_state_changed), self);
		}

	}

	self->verify_server = TRUE;
	self->verified_server = FALSE;
	self->session = NULL;

	/* Need the soup session before the state changed */
	self->session = soup_session_new();

	nm_state_changed(self->nm_client, NULL, self);
	uccs_notify_state_change(self);

	return;
}

/* Small function to try and figure out the state of the server and notify of
   status changes appropriately */
void
uccs_notify_state_change (UccsServer * server)
{
	ServerState tempstate = SERVER_STATE_ALLGOOD;

	if (server->last_network < server->min_network) {
		tempstate = SERVER_STATE_UNAVAILABLE;
	}

	if (server->verify_server && !server->verified_server && server->min_network > NM_STATE_DISCONNECTED) {
		tempstate = SERVER_STATE_UNAVAILABLE;
	}

	if (tempstate != server->parent.state) {
		server->parent.state = tempstate;
		g_signal_emit_by_name(server, SERVER_SIGNAL_STATE_CHANGED, server->parent.state);
	}

	return;
}

struct _hash_helper {
	GVariant * params;
	GDBusConnection * session;
};

/* GHashTable foreach item */
static gboolean
clear_hash_helper (gpointer key, gpointer value, gpointer user_data)
{
	struct _hash_helper * helper = (struct _hash_helper *)user_data;
	GError * error = NULL;

	g_dbus_connection_emit_signal(helper->session,
	                              (const gchar *)key, /* dest */
	                              "/org/ArcticaProject/RemoteLogon", /* object path */
	                              "org.ArcticaProject.RemoteLogon", /* interface name */
	                              "org.ArcticaProject.RemoteLogon.LoginChanged", /* signal name */
	                              helper->params, /* params */
	                              &error);

	if (error != NULL) {
		g_warning("Unable to signal UCCS server shutdown: %s", error->message);
		g_error_free(error);
	}

	return TRUE;
}

/* Clear the hash table by going through it and signaling */
static void
clear_hash (UccsServer * server)
{
	if (g_hash_table_size(server->lovers) == 0) {
		return;
	}

	g_return_if_fail(server->parent.uri != NULL);
	g_return_if_fail(server->username != NULL);

	GDBusConnection * session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL); /* Shouldn't block, we should have it */
	GVariant * param = g_variant_new("(ss)", server->parent.uri, server->username); /* params */
	g_variant_ref_sink(param);

	struct _hash_helper helper;
	helper.params = param;
	helper.session = session;

	g_hash_table_foreach_remove(server->lovers, clear_hash_helper, &helper);

	g_object_unref(session);
	g_variant_unref(param);

	return;
}

/* Clear the JSON task and waiters */
static void
clear_json (UccsServer * self)
{
	if (self->json_watch != 0) {
		g_source_remove(self->json_watch);
		self->json_watch = 0;
	}

	if (self->json_pid != 0) {
		g_spawn_close_pid(self->json_pid);
		self->json_pid = 0;
	}

	if (self->json_stream != NULL) {
		g_input_stream_close(self->json_stream, NULL, NULL);
		g_object_unref(self->json_stream);
		self->json_stream = NULL;
	}

	if (self->pass_stream != NULL) {
		g_output_stream_close(self->pass_stream, NULL, NULL);
		g_object_unref(self->pass_stream);
		self->pass_stream = NULL;
	}

	json_waiters_notify(self, FALSE);

	return;
}

/* Clean up references */
static void
uccs_server_dispose (GObject *object)
{
	UccsServer * self = UCCS_SERVER(object);

	g_clear_object(&self->session);

	if (self->nm_signal != 0) {
		g_signal_handler_disconnect(self->nm_client, self->nm_signal);
		self->nm_signal = 0;
	}

	g_clear_object(&self->nm_client);

	clear_json(self);

	if (self->lovers != NULL) {
		clear_hash(self);
	}

	g_list_free_full(self->subservers, g_object_unref);
	self->subservers = NULL; /* Ironically the free function is the only GList
	                      function that doesn't return a new pointer by itself */

	G_OBJECT_CLASS (uccs_server_parent_class)->dispose (object);
	return;
}

/* Clean up memory */
static void
uccs_server_finalize (GObject *object)
{
	UccsServer * self = UCCS_SERVER(object);

	g_free(self->exec); self->exec = NULL;
	g_free(self->username); self->username = NULL;
	g_free(self->password); self->password = NULL;

	if (self->lovers != NULL) {
		g_hash_table_unref(self->lovers);
		self->lovers = NULL;
	}

	G_OBJECT_CLASS (uccs_server_parent_class)->finalize (object);
	return;
}

/* Callback from the message getting complete */
static void
verify_server_cb (SoupSession * session, SoupMessage * message, gpointer user_data)
{
	UccsServer * server = UCCS_SERVER(user_data);
	guint statuscode = 404;

	g_object_get(G_OBJECT(message), SOUP_MESSAGE_STATUS_CODE, &statuscode, NULL);
	g_debug("Verification came back with status: %d", statuscode);

	if (statuscode == 200) {
		server->verified_server = TRUE;
	} else {
		server->verified_server = FALSE;
	}

	uccs_notify_state_change(server);

	return;
}

/* Set up the process to verify the server */
static void
verify_server (UccsServer * server)
{
	g_return_if_fail(server->session != NULL);

	if (server->parent.uri == NULL) {
		return;
	}

	SoupMessage * message = soup_message_new("HEAD", server->parent.uri);
	soup_session_queue_message(server->session, message, verify_server_cb, server);
	g_debug("Getting HEAD from: %s", server->parent.uri);

	return;
}

/* Callback for when the Network Manger state changes */
static void
nm_state_changed (NMClient *client, const GParamSpec *pspec, gpointer user_data)
{
	g_return_if_fail(IS_UCCS_SERVER(user_data));
	UccsServer * server = UCCS_SERVER(user_data);

	if (server->nm_client == NULL || !nm_client_get_nm_running(server->nm_client)) {
		server->last_network = NM_STATE_DISCONNECTED;
	} else {
		server->last_network = nm_client_get_state(server->nm_client);
	}

	if (server->last_network == NM_STATE_DISCONNECTED) {
		server->verified_server = FALSE;
		soup_session_abort(server->session);
	}

	if (server->last_network == NM_STATE_CONNECTED_GLOBAL && server->verify_server && !server->verified_server) {
		verify_server(server);
	}

	uccs_notify_state_change(server);

	return;
}

/* Get the properties that can be sent by the greeter for this server */
static GVariant *
get_properties (Server * server)
{
	GVariantBuilder propbuilder;
	g_variant_builder_init(&propbuilder, G_VARIANT_TYPE_ARRAY);

	GVariantBuilder namebuilder;
	g_variant_builder_init(&namebuilder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(&namebuilder, g_variant_new_string("email"));
	g_variant_builder_add_value(&namebuilder, g_variant_new_boolean(TRUE));
	g_variant_builder_add_value(&namebuilder, g_variant_new_variant(g_variant_new_string("")));
	g_variant_builder_add_value(&namebuilder, g_variant_parse(G_VARIANT_TYPE_VARDICT, "{}", NULL, NULL, NULL));
	g_variant_builder_add_value(&propbuilder, g_variant_builder_end(&namebuilder));

	GVariantBuilder passbuilder;
	g_variant_builder_init(&passbuilder, G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add_value(&passbuilder, g_variant_new_string("password"));
	g_variant_builder_add_value(&passbuilder, g_variant_new_boolean(TRUE));
	g_variant_builder_add_value(&passbuilder, g_variant_new_variant(g_variant_new_string("")));
	g_variant_builder_add_value(&passbuilder, g_variant_parse(G_VARIANT_TYPE_VARDICT, "{}", NULL, NULL, NULL));
	g_variant_builder_add_value(&propbuilder, g_variant_builder_end(&passbuilder));

	return g_variant_builder_end(&propbuilder);
}

/* Set the exec value for the server */
const gchar *
uccs_server_set_exec (UccsServer * server, const gchar * exec)
{
	g_return_val_if_fail(IS_UCCS_SERVER(server), NULL);

	g_clear_pointer(&server->exec, g_free);
	server->exec = g_find_program_in_path(exec);

	if (server->exec == NULL) {
		g_warning ("unable to find program %s", exec);
	}

	return server->exec;
}

/* Build a new uccs server from a keyfile and a group in it */
Server *
uccs_server_new_from_keyfile (GKeyFile * keyfile, const gchar * groupname)
{
	g_return_val_if_fail(keyfile != NULL, NULL); /* NOTE: No way to check if that's really a keyfile :-( */
	g_return_val_if_fail(groupname != NULL, NULL);

	if (!g_key_file_has_group(keyfile, groupname)) {
		g_warning("Server specified but group '%s' was not found", groupname);
		return NULL;
	}

	UccsServer * server = g_object_new(UCCS_SERVER_TYPE, NULL);

	if (g_key_file_has_key(keyfile, groupname, CONFIG_SERVER_NAME, NULL)) {
		gchar * keyname = g_key_file_get_string(keyfile, groupname, CONFIG_SERVER_NAME, NULL);
		server->parent.name = g_strdup(_(keyname));
		g_free(keyname);
	}

	if (g_key_file_has_key(keyfile, groupname, CONFIG_SERVER_URI, NULL)) {
		server->parent.uri = g_key_file_get_string(keyfile, groupname, CONFIG_SERVER_URI, NULL);
	}

	if (g_key_file_has_key(keyfile, groupname, CONFIG_UCCS_EXEC, NULL)) {
		gchar * key = g_key_file_get_string(keyfile, groupname, CONFIG_UCCS_EXEC, NULL);
		uccs_server_set_exec(server, key);
		g_free(key);
	}

	if (g_key_file_has_key(keyfile, groupname, CONFIG_UCCS_NETWORK, NULL)) {
		gchar * key = g_key_file_get_string(keyfile, groupname, CONFIG_UCCS_NETWORK, NULL);

		if (g_strcmp0(key, CONFIG_UCCS_NETWORK_NONE) == 0) {
			server->min_network = NM_STATE_DISCONNECTED;
		} else if (g_strcmp0(key, CONFIG_UCCS_NETWORK_GLOBAL) == 0) {
			server->min_network = NM_STATE_CONNECTED_GLOBAL;
		}
		/* NOTE: There is a possibility for other network types to be added here,
		   but they can be tricky to test.  Feel free to patch it, but please include
		   those tests :-) */

		g_free(key);
	}

	if (g_key_file_has_key(keyfile, groupname, CONFIG_UCCS_VERIFY, NULL)) {
		server->verify_server = g_key_file_get_boolean(keyfile, groupname, CONFIG_UCCS_VERIFY, NULL);
	}

	nm_state_changed(server->nm_client, NULL, server);
	uccs_notify_state_change(server);

	return SERVER(server);
}

/* Look at the array of RLS data and build a server for each entry
   in the array */
static gboolean
parse_rds_array (UccsServer * server, JsonArray * array)
{
	// Got a new set of servers, delete the old one
	g_list_free_full(server->subservers, g_object_unref);
	server->subservers = NULL;

	int i;
	for (i = 0; i < json_array_get_length(array); i++) {
		JsonNode * node = json_array_get_element(array, i);

		if (JSON_NODE_TYPE(node) != JSON_NODE_OBJECT) {
			continue;
		}

		JsonObject * object = json_node_get_object(node);
		Server * newserver = server_new_from_json(object);
		if (newserver != NULL) {
			server->subservers = g_list_append(server->subservers, newserver);
		}
	}

	return TRUE;
}

/* Parse the JSON content and allocate servers based on that */
static gboolean
parse_json (UccsServer * server, GInputStream * json)
{
	if (json == NULL) return FALSE; /* Shouldn't happen, but let's just handle it */

	gboolean passed = TRUE;
	JsonParser * parser = json_parser_new();
	GError * error = NULL;

	if (!json_parser_load_from_stream(parser, json, NULL, &error)) {
		g_warning("Unable to parse JSON data: %s", error->message);
		g_error_free(error);
		error = NULL;
		passed = FALSE;
	}

	/* Make sure we have a sane root node */
	JsonNode * root_node = NULL;
	if (passed) {
		root_node = json_parser_get_root(parser);

#if 0
		JsonGenerator * gen = json_generator_new();
		json_generator_set_root(gen, root_node);
		gchar * data = json_generator_to_data(gen, NULL);
		g_debug("%s", data);
		g_free(data);
		g_object_unref(G_OBJECT(gen));
#endif
	}
	if (root_node != NULL && JSON_NODE_TYPE(root_node) != JSON_NODE_OBJECT) {
		g_warning("Root node of JSON data is not an object.  It is: %s", json_node_type_name(root_node));
		passed = FALSE;
	}

	/* Take our object and see if it has the property that we need */
	JsonObject * root_object = NULL;
	if (passed) {
		root_object = json_node_get_object(root_node);
	}
	if (root_object != NULL && json_object_has_member(root_object, "RemoteDesktopServers")) {
		/* This shows that we have some.  It's okay if there aren't any.  Seems
		   a bit silly, but we're not bitching too much. */
		JsonNode * rds_node = json_object_get_member(root_object, "RemoteDesktopServers");
		if (JSON_NODE_TYPE(rds_node) == JSON_NODE_ARRAY) {
			JsonArray * rds_array = json_node_get_array(rds_node);
			passed = parse_rds_array(server, rds_array);
		} else {
			/* Okay we're a little bit angrier about this one */
			g_warning("Malformed 'RemoteDesktopServer' entry.  Not an array but a: %s", json_node_type_name(rds_node));
			passed = FALSE;
		}

		if (json_object_has_member(root_object, "DefaultServer")) {
			JsonNode * ds_node = json_object_get_member(root_object, "DefaultServer");
			if (JSON_NODE_TYPE(ds_node) == JSON_NODE_VALUE && json_node_get_value_type(ds_node) == G_TYPE_STRING) {
				const gchar * default_server_name = json_node_get_string(ds_node);
				if (default_server_name != NULL) {
					GList * lserver;
					for (lserver = server->subservers; lserver != NULL; lserver = g_list_next(lserver)) {
						Server * serv = SERVER(lserver->data);

						if (g_strcmp0(serv->name, default_server_name) == 0) {
							serv->last_used = TRUE;
							break;
						}
					}
					if (lserver == NULL && strlen(default_server_name) > 0) {
						g_warning("Could not find the 'DefaultServer' server.");
						passed = FALSE;
					}
				}
			} else {
				g_warning("Malformed 'DefaultServer' entry.  Not a string value");
				passed = FALSE;
			}
		}
	} else {
		g_debug("No 'RemoteDesktopServers' found");
	}

	g_object_unref(parser);
	return passed;
}

/* Go through the waiters and notify them of the status */
static void
json_waiters_notify (UccsServer * server, gboolean unlocked)
{
	/* NOTE: Taking the list as the call back might add themselves to
	   the list so we don't want to have it corrupted in the middle of
	   the execution of this function */
	GList * waiters = server->json_waiters;
	server->json_waiters = NULL;

	while (waiters != NULL) {
		json_callback_t * json_callback = (json_callback_t *)waiters->data;

		if (unlocked) {
			g_hash_table_insert(server->lovers, g_strdup(json_callback->sender), GINT_TO_POINTER(TRUE));
		}

		if (json_callback->callback != NULL) {
			json_callback->callback(server, unlocked, json_callback->userdata);
		}

		g_free(json_callback->sender);
		g_free(json_callback);
		waiters = g_list_delete_link(waiters, waiters);
	}

	return;
}

/* Callback from when we know that we've got all the JSON we're
   gonna get */
static void
json_grab_cb (GPid pid, gint status, gpointer user_data)
{
	UccsServer * server = UCCS_SERVER(user_data);

	server->json_pid = 0;
	server->json_watch = 0;

	if (status == 0) {
		gboolean parser = parse_json(server, server->json_stream);

		json_waiters_notify(server, parser);
	} else {
		g_free(server->username);
		server->username = NULL;
		g_free(server->password);
		server->password = NULL;

		json_waiters_notify(server, FALSE);
	}

	/* Drop the Streams -- NOTE: DO NOT CROSS THE STREAMS */
	g_output_stream_close(server->pass_stream, NULL, NULL);
	g_object_unref(server->pass_stream);
	server->pass_stream = NULL;

	g_input_stream_close(server->json_stream, NULL, NULL);
	g_object_unref(server->json_stream);
	server->json_stream = NULL;

	g_spawn_close_pid(pid);

	return;
}

/* This is the callback for writing the password, it mostly exists
   to free the buffer, but we'll print some info as well just because
   we can */
static void
password_write_cb (GObject * src_obj, GAsyncResult * res, gpointer user_data)
{
	/* Kill the buffer */
	g_free(user_data);

	GError * error = NULL;
	g_output_stream_write_finish(G_OUTPUT_STREAM(src_obj), res, &error);

	if (error != NULL) {
		g_warning("Unable to write password to UCCS process: %s", error->message);
		g_error_free(error);
	} else {
		g_debug("Wrote password to UCCS process");
		g_output_stream_close(G_OUTPUT_STREAM(src_obj), NULL, NULL);
	}

	return;
}

/**
 * uccs_server_unlock:
 * @server: The server to unlock
 * @address: DBus address of the person unlocking us
 * @username: Username for the UCCS
 * @password: (allow-none) Password to use
 * @allowcache: If using cache is allowed
 * @callback: Function to call when we have an answer
 * @user_data: Data for the callback
 *
 * Unlocks the UCCS server making servers available either from the
 * cache or from the network.
 */
void
uccs_server_unlock (UccsServer * server, const gchar * address, const gchar * username, const gchar * password, gboolean allowcache, void (*callback) (UccsServer * server, gboolean unlocked, gpointer user_data), gpointer user_data)
{
	g_return_if_fail(IS_UCCS_SERVER(server));
	g_return_if_fail(username != NULL);
	g_return_if_fail(address != NULL);

	/* Check the current values we have, they might be NULL, which in
	   that case they won't match */
	if (allowcache && g_strcmp0(username, server->username) == 0 &&
			g_strcmp0(password, server->password) == 0) {
		g_hash_table_insert(server->lovers, g_strdup(address), GINT_TO_POINTER(TRUE));

		if (callback != NULL) {
			callback(server, TRUE, user_data);
		}

		return;
	}

	g_return_if_fail(server->exec != NULL); /* Shouldn't happen, but I'd feel safer if we checked */

	/* If we're not going to allow the cache, just clear it right away */
	if (!allowcache) {
		clear_hash(server);
	}

	/* We're changing the username and password, if there were other
	   people who had it, they need to know we're different now */
	if (g_strcmp0(username, server->username) != 0 ||
			g_strcmp0(password, server->password) != 0) {
		clear_hash(server);
		clear_json(server);

		g_clear_pointer(&server->username, g_free);
		g_clear_pointer(&server->password, g_free);

		server->username = g_strdup(username);
		server->password = g_strdup(password);
	}

	/* Add ourselves to the queue */
	json_callback_t * json_callback = g_new0(json_callback_t, 1);
	json_callback->sender = g_strdup(address);
	json_callback->callback = callback;
	json_callback->userdata = user_data;

	server->json_waiters = g_list_append(server->json_waiters, json_callback);

	if (server->json_pid == 0) {
		gint std_in, std_out;
		GError * error = NULL;

		const gchar * argv[3];
		argv[0] = server->exec;
		argv[1] = server->username;
		argv[2] = NULL;

		g_setenv("SERVER_ROOT", server->parent.uri, TRUE);
		g_setenv("API_VERSION", UCCS_API_VERSION, TRUE);

		g_spawn_async_with_pipes(NULL, /* pwd */
		                         (gchar **)argv,
		                         NULL, /* env */
		                         G_SPAWN_DO_NOT_REAP_CHILD,
		                         NULL, NULL, /* child setup */
		                         &server->json_pid,
		                         &std_in,
		                         &std_out,
		                         NULL, /* stderr */
		                         &error); /* error */

		if (error != NULL) {
			g_warning("Unable to start UCCS process: %s", error->message);
			g_error_free(error);
			server->json_pid = 0; /* really shouldn't get changed, but since we're using it to detect if it's running, let's double check, eh? */
			json_waiters_notify(server, FALSE);
		} else {
			/* Watch for when it's done */
			server->json_watch = g_child_watch_add(server->json_pid, json_grab_cb, server);

			/* Set up I/O streams */
			server->json_stream = g_unix_input_stream_new(std_out, TRUE);
			server->pass_stream = g_unix_output_stream_new(std_in, TRUE);

			gchar * pass = g_strdup(server->password);
			g_output_stream_write_async(server->pass_stream,
			                            pass,
			                            strlen(pass), /* number of bytes */
			                            G_PRIORITY_DEFAULT, /* priority */
			                            NULL, /* cancellable */
			                            password_write_cb,
			                            pass);
		}
	}

	return;
}

/* A little quickie function to handle the null server array */
inline static GVariant *
null_server_array (void)
{
	return g_variant_new_array(G_VARIANT_TYPE("(sssba(sbva{sv})a(si))"),
		NULL, 0);
}

/**
 * uccs_server_get_servers:
 * @server: Server to get our list from
 * @address: Who's asking
 *
 * Will get a valid variant with servers.  If the asker hasn't unlocked us
 * then the list will always be empty.
 *
 * Return value: A variant array
 */
GVariant *
uccs_server_get_servers (UccsServer * server, const gchar * address)
{
	g_return_val_if_fail(IS_UCCS_SERVER(server), null_server_array());
	g_return_val_if_fail(address != NULL, null_server_array());

	if (!GPOINTER_TO_INT(g_hash_table_lookup(server->lovers, address))) {
		g_warning("Address '%s' is not authorized", address);
		return null_server_array();
	}

	gchar *last_used_server_name = NULL;
	if (server->username != NULL && server->password != NULL) {
		gchar *username_sha = g_compute_checksum_for_string (G_CHECKSUM_SHA256, server->username, -1);
		gchar *file_path = g_build_path ("/", g_get_user_cache_dir(), "remote-logon-service", "cache", username_sha, NULL);
		gchar *encryptedContents;
		gsize encryptedContentsLength;
		if (g_file_get_contents (file_path, &encryptedContents, &encryptedContentsLength, NULL)) {
			gchar *file_contents = do_aes_decrypt(encryptedContents, server->password, encryptedContentsLength);
			g_free (encryptedContents);
			if (file_contents != NULL) {
				GKeyFile * key_file = g_key_file_new();
				if (g_key_file_load_from_data (key_file, file_contents, strlen (file_contents), G_KEY_FILE_NONE, NULL)) {
					last_used_server_name = g_key_file_get_string (key_file, server->parent.name, "last_used", NULL);
				}
				g_key_file_free (key_file);
				g_free (file_contents);
			}
		}
		g_free (username_sha);
		g_free (file_path);
	}

	GVariantBuilder array;
	g_variant_builder_init(&array, G_VARIANT_TYPE_ARRAY);
	GList * lserver;
	gint servercnt = 0;

	Server * last_used_server = NULL;
	if (last_used_server_name != NULL) {
		for (lserver = server->subservers; last_used_server == NULL && lserver != NULL; lserver = g_list_next(lserver)) {
			Server * serv = SERVER(lserver->data);

			/* We only want servers that are all good */
			if (serv->state != SERVER_STATE_ALLGOOD) {
				continue;
			}
			if (g_strcmp0(serv->name, last_used_server_name) == 0)
				last_used_server = serv;
		}
	}
	g_free (last_used_server_name);

	for (lserver = server->subservers; lserver != NULL; lserver = g_list_next(lserver)) {
		Server * serv = SERVER(lserver->data);

		/* We only want servers that are all good */
		if (serv->state != SERVER_STATE_ALLGOOD) {
			continue;
		}

		if (last_used_server != NULL)
			serv->last_used = last_used_server == serv;

		servercnt++;
		g_variant_builder_add_value(&array, server_get_variant(serv));
	}

	if (servercnt == 0) {
		g_variant_builder_clear(&array);
		return null_server_array();
	}

	return g_variant_builder_end(&array);
}

/* Returns the cached domains as an array.  Not currently with any of
   this cached currently, so it's a NULL array. */
static GVariant *
get_cached_domains (Server * server)
{
	g_return_val_if_fail(IS_UCCS_SERVER(server), NULL);
	return g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
}

/* Tail recursive function to look at a list entry and see
   if that server matches a URI, or go down the list */
static Server *
find_uri_helper (GList * list, const gchar * uri)
{
	if (list == NULL) return NULL;

	Server * inserver = SERVER(list->data);

	if (inserver == NULL) {
		return find_uri_helper(g_list_next(list), uri);
	}

	Server * outserver = server_find_uri(inserver, uri);

	if (outserver != NULL) {
		return outserver;
	}

	return find_uri_helper(g_list_next(list), uri);
}

/* Look through our subservers to see if any of them match this
   URI either */
static Server *
find_uri (Server * server, const gchar * uri)
{
	g_return_val_if_fail(IS_UCCS_SERVER(server), NULL);
	/* If it is this server that's handled by the super class */
	return find_uri_helper(UCCS_SERVER(server)->subservers, uri);
}

/* Look through our subservers to see if any of them match this
   URI and set as used */
static void
set_last_used_server (Server * server, const gchar * uri)
{
	Server * subserver = server_find_uri(server, uri);

	if (subserver != NULL) {
		subserver->last_used = TRUE;

		/* Write to disk */
		if (UCCS_SERVER(server)->username != NULL && UCCS_SERVER(server)->password) {
			GKeyFile * key_file = g_key_file_new();
			g_key_file_set_string (key_file, server->name, "last_used", subserver->name);
			gsize data_length;
			gchar *data = g_key_file_to_data (key_file, &data_length, NULL);
			g_key_file_free (key_file);

			size_t enc_data_length;
			gchar *enc_data = do_aes_encrypt(data, UCCS_SERVER(server)->password, &enc_data_length);
			g_free (data);

			gchar *dir_path = g_build_path ("/", g_get_user_cache_dir(), "remote-logon-service", "cache", NULL);
			gint status = g_mkdir_with_parents (dir_path, 0700);
			if (status == 0)
			{
				gchar *username_sha = g_compute_checksum_for_string (G_CHECKSUM_SHA256, UCCS_SERVER(server)->username, -1);
				gchar *path = g_build_path ("/", dir_path, username_sha, NULL);
				gboolean success = g_file_set_contents (path, enc_data, enc_data_length, NULL);
				if (!success) {
					g_warning("Failed writing cache data to '%s'.", path);
				}
				g_free (username_sha);
				g_free (path);
			}
			else
			{
				g_warning("Failed to create '%s'.", dir_path);
			}
			g_free (enc_data);
			g_free (dir_path);
		}
	}
}
