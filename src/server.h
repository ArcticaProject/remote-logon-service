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

#ifndef __SERVER_H__
#define __SERVER_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define SERVER_TYPE            (server_get_type ())
#define SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SERVER_TYPE, Server))
#define SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SERVER_TYPE, ServerClass))
#define IS_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SERVER_TYPE))
#define IS_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SERVER_TYPE))
#define SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SERVER_TYPE, ServerClass))

#define SERVER_SIGNAL_STATE_CHANGED  "state-changed"

typedef struct _Server      Server;
typedef struct _ServerClass ServerClass;
typedef enum   _ServerState ServerState;

enum _ServerState {
	SERVER_STATE_ALLGOOD,
	SERVER_STATE_UNAVAILABLE
};

struct _ServerClass {
	GObjectClass parent_class;
	GVariant * (*get_properties) (Server * server);
	GVariant * (*get_applications) (Server * server);
	GVariant * (*get_domains) (Server * server);
	Server * (*find_uri) (Server * server, const gchar * uri);
	void (*set_last_used_server) (Server * server, const gchar * uri);

	/* signals */
	void (*state_changed) (Server * server, ServerState newstate, gpointer user_data);
};

struct _Server {
	GObject parent;

	gchar * name;
	gchar * uri;
	gboolean last_used;

	ServerState state;
};

GType server_get_type (void);
Server * server_new_from_keyfile (GKeyFile * keyfile, const gchar * group);
Server * server_new_from_json (JsonObject * object);
GVariant * server_get_variant (Server * server);
GVariant * server_cached_domains (Server * server);
Server * server_find_uri (Server * server, const gchar * uri);
void server_set_last_used_server (Server * server, const gchar * uri);

G_END_DECLS

#endif
