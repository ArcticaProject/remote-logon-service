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

#include "server.h"
#include "defines.h"
#include "citrix-server.h"
#include "rdp-server.h"
#include "uccs-server.h"
#include "x2go-server.h"

static void server_class_init (ServerClass *klass);
static void server_init       (Server *self);
static void server_dispose    (GObject *object);
static void server_finalize   (GObject *object);

/* Signals */
enum {
	STATE_CHANGED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (Server, server, G_TYPE_OBJECT);

static guint signals[LAST_SIGNAL] = { 0 };

static void
server_class_init (ServerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = server_dispose;
	object_class->finalize = server_finalize;

	signals[STATE_CHANGED] = g_signal_new(SERVER_SIGNAL_STATE_CHANGED,
	                                      G_TYPE_FROM_CLASS(klass),
	                                      G_SIGNAL_RUN_LAST,
	                                      G_STRUCT_OFFSET(ServerClass, state_changed),
	                                      NULL, NULL,
	                                      g_cclosure_marshal_VOID__INT,
	                                      G_TYPE_NONE, 1, G_TYPE_INT, G_TYPE_NONE);

	return;
}

static void
server_init (Server *self)
{
	self->name = NULL;
	self->uri = NULL;
	self->last_used = FALSE;
	self->state = SERVER_STATE_ALLGOOD;

	return;
}

static void
server_dispose (GObject *object)
{

	G_OBJECT_CLASS (server_parent_class)->dispose (object);
	return;
}

static void
server_finalize (GObject *object)
{
	Server * server = SERVER(object);

	g_free(server->name);
	g_free(server->uri);

	G_OBJECT_CLASS (server_parent_class)->finalize (object);
	return;
}

/**
 * server_new_from_keyfile:
 * @keyfile: The keyfile with the @group in it to define the server
 * @group: Group name for this server
 *
 * Looks at a group within the keyfile and builds a server based on
 * it's type.  Mostly works with a subclass based on the type.
 *
 * Return value: A new Server object or NULL if error
 */
Server *
server_new_from_keyfile (GKeyFile * keyfile, const gchar * group)
{
	g_return_val_if_fail(keyfile != NULL, NULL);
	g_return_val_if_fail(group != NULL, NULL);

	if (!g_key_file_has_group(keyfile, group)) {
		return NULL;
	}

	gchar * type = NULL;
	if (g_key_file_has_key(keyfile, group, CONFIG_SERVER_TYPE, NULL)) {
		type = g_key_file_get_string(keyfile, group, CONFIG_SERVER_TYPE, NULL);
	}

	if (g_strcmp0(type, CONFIG_SERVER_TYPE_RDP) == 0) {
		return SERVER(rdp_server_new_from_keyfile(keyfile, group));
	} else if (g_strcmp0(type, CONFIG_SERVER_TYPE_ICA) == 0) {
		return SERVER(citrix_server_new_from_keyfile(keyfile, group));
	} else {
		return SERVER(uccs_server_new_from_keyfile(keyfile, group));
	}

	return NULL;
}

/**
 * server_new_from_json:
 * @object: JSON object with server definition
 *
 * Looks at the type and then uses a subclassed function to build the
 * server.
 *
 * Return value: A new Server object or NULL if error
 */
Server *
server_new_from_json (JsonObject * object)
{
	g_return_val_if_fail(object != NULL, NULL);

	if (!json_object_has_member(object, "Protocol")) {
		return NULL;
	}

	JsonNode * proto_node = json_object_get_member(object, "Protocol");
	if (JSON_NODE_TYPE(proto_node) != JSON_NODE_VALUE) {
		return NULL;
	}
	if (json_node_get_value_type(proto_node) != G_TYPE_STRING) {
		return NULL;
	}

	const gchar * proto = json_node_get_string(proto_node);
	Server * newserver = NULL;

	if (g_strcmp0(proto, "ICA") == 0 || g_strcmp0(proto, "ica") == 0) {
		newserver = citrix_server_new_from_json(object);
	}
	else if (g_strcmp0(proto, "freerdp") == 0 || g_strcmp0(proto, "rdp") == 0 || g_strcmp0(proto, "freerdp2") == 0 || g_strcmp0(proto, "RDP") == 0 || g_strcmp0(proto, "FreeRDP") == 0 || g_strcmp0(proto, "FreeRDP2") == 0) {
		newserver = rdp_server_new_from_json(object);
	}
	else if (g_strcmp0(proto, "x2go") == 0 || g_strcmp0(proto, "X2go") == 0 || g_strcmp0(proto, "X2Go") == 0 || g_strcmp0(proto, "X2GO") == 0 || g_strcmp0(proto, "x2GO") == 0 || g_strcmp0(proto, "x2gO") == 0) {
		newserver = x2go_server_new_from_json(object);
	}

	return newserver;
}

GVariant *
server_get_variant (Server * server)
{
	/* Okay, this doesn't do anything useful, but it will generate an error
	   which could be a good thing */
	g_return_val_if_fail(IS_SERVER(server), NULL);

	ServerClass * klass = SERVER_GET_CLASS(server);
	if (klass->get_properties != NULL) {
		GVariantBuilder tuple;
		g_variant_builder_init(&tuple, G_VARIANT_TYPE_TUPLE);

		if (IS_CITRIX_SERVER(server)) {
			g_variant_builder_add_value(&tuple, g_variant_new_string("ica"));
		} else if (IS_RDP_SERVER(server)) {
			g_variant_builder_add_value(&tuple, g_variant_new_string("freerdp2"));
		} else if (IS_UCCS_SERVER(server)) {
			g_variant_builder_add_value(&tuple, g_variant_new_string("uccs"));
		} else if (IS_X2GO_SERVER(server)) {
			g_variant_builder_add_value(&tuple, g_variant_new_string("x2go"));
		} else {
			g_assert_not_reached();
		}

		if (server->name != NULL) {
			g_variant_builder_add_value(&tuple, g_variant_new_string(server->name));
		} else {
			g_warning("Server has no name");
			g_variant_builder_add_value(&tuple, g_variant_new_string(""));
		}

		if (server->uri != NULL) {
			g_variant_builder_add_value(&tuple, g_variant_new_string(server->uri));
		} else {
			g_warning("Server has no URI");
			g_variant_builder_add_value(&tuple, g_variant_new_string(""));
		}

		g_variant_builder_add_value(&tuple, g_variant_new_boolean(server->last_used));

		GVariant * props = klass->get_properties(server);
		g_variant_builder_add_value(&tuple, props);

		if (klass->get_applications != NULL) {
			GVariant * array = klass->get_applications(server);
			g_variant_builder_add_value(&tuple, array);
		} else {
			/* NULL array of applications */
			g_variant_builder_add_value(&tuple, g_variant_new_array(G_VARIANT_TYPE("(si)"), NULL, 0));
		}

		return g_variant_builder_end(&tuple);
	}

	return NULL;
}

/**
 * server_cached_domains:
 * @server: Where should we find those domains?
 *
 * Gets a list of cached domains for a particular server, if this function
 * isn't overriden, then a null array is returned.
 */
GVariant *
server_cached_domains (Server * server)
{
	g_return_val_if_fail(IS_SERVER(server), NULL);

	ServerClass * klass = SERVER_GET_CLASS(server);
	if (klass->get_domains != NULL) {
		return klass->get_domains(server);
	}

	return g_variant_new_array(G_VARIANT_TYPE_STRING, NULL, 0);
}

/**
 * server_find_uri:
 * @server: Server to look in
 * @uri: URI to search for
 *
 * Checks the URI of this server to see if it matches, and then look
 * to see if subclasses have a way to match it another way.
 */
Server *
server_find_uri (Server * server, const gchar * uri)
{
	g_return_val_if_fail(IS_SERVER(server), NULL);

	if (g_strcmp0(server->uri, uri) == 0) {
		return server;
	}

	ServerClass * klass = SERVER_GET_CLASS(server);

	if (klass->find_uri != NULL) {
		return klass->find_uri(server, uri);
	}

	return NULL;
}

/**
 * server_set_last_used:
 * @server: Server to look in
 * @uri: URI to set as last used
 *
 * Checks the URI of this server to see if it matches, and then look
 * to see if subclasses have a way to match it another way.
 */
void
server_set_last_used_server (Server * server, const gchar * uri)
{
	g_return_if_fail(IS_SERVER(server));

	if (g_strcmp0(server->uri, uri) == 0) {
		server->last_used = TRUE;
	} else {
		ServerClass * klass = SERVER_GET_CLASS(server);

		if (klass->set_last_used_server != NULL) {
			klass->set_last_used_server(server, uri);
		}
	}
}
