/*
 * Copyright © 2012 Canonical Ltd.
 * Copyright © 2013 Mike Gabriel <mike.gabriel@das-netzwerkteam.de>
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

#ifndef __X2GO_SERVER_H__
#define __X2GO_SERVER_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "server.h"

G_BEGIN_DECLS

#define X2GO_SERVER_TYPE            (x2go_server_get_type ())
#define X2GO_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), X2GO_SERVER_TYPE, X2GoServer))
#define X2GO_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), X2GO_SERVER_TYPE, X2GoServerClass))
#define IS_X2GO_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), X2GO_SERVER_TYPE))
#define IS_X2GO_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), X2GO_SERVER_TYPE))
#define X2GO_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), X2GO_SERVER_TYPE, X2GoServerClass))

typedef struct _X2GoServer      X2GoServer;
typedef struct _X2GoServerClass X2GoServerClass;

struct _X2GoServerClass {
       ServerClass parent_class;
};

struct _X2GoServer {
       Server parent;

       gchar * username;
       gchar * password;
       gchar * sessiontype;
       gboolean sessiontype_required;
};

GType x2go_server_get_type (void);
Server * x2go_server_new_from_keyfile (GKeyFile * keyfile, const gchar * name);
Server * x2go_server_new_from_json (JsonObject * object);

G_END_DECLS

#endif
