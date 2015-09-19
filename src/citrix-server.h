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

#ifndef __CITRIX_SERVER_H__
#define __CITRIX_SERVER_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "server.h"

G_BEGIN_DECLS

#define CITRIX_SERVER_TYPE            (citrix_server_get_type ())
#define CITRIX_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CITRIX_SERVER_TYPE, CitrixServer))
#define CITRIX_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CITRIX_SERVER_TYPE, CitrixServerClass))
#define IS_CITRIX_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CITRIX_SERVER_TYPE))
#define IS_CITRIX_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CITRIX_SERVER_TYPE))
#define CITRIX_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CITRIX_SERVER_TYPE, CitrixServerClass))

typedef struct _CitrixServer      CitrixServer;
typedef struct _CitrixServerClass CitrixServerClass;

struct _CitrixServerClass {
	ServerClass parent_class;
};

struct _CitrixServer {
	Server parent;

	gchar * username;
	gchar * password;
	gchar * domain;
	gboolean domain_required;
};

GType citrix_server_get_type (void);
Server * citrix_server_new_from_keyfile (GKeyFile * keyfile, const gchar * name);
Server * citrix_server_new_from_json (JsonObject * object);

G_END_DECLS

#endif
