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

#ifndef __RDP_SERVER_H__
#define __RDP_SERVER_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "server.h"

G_BEGIN_DECLS

#define RDP_SERVER_TYPE            (rdp_server_get_type ())
#define RDP_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RDP_SERVER_TYPE, RdpServer))
#define RDP_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RDP_SERVER_TYPE, RdpServerClass))
#define IS_RDP_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RDP_SERVER_TYPE))
#define IS_RDP_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RDP_SERVER_TYPE))
#define RDP_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RDP_SERVER_TYPE, RdpServerClass))

typedef struct _RdpServer      RdpServer;
typedef struct _RdpServerClass RdpServerClass;

struct _RdpServerClass {
	ServerClass parent_class;
};

struct _RdpServer {
	Server parent;

	gchar * username;
	gchar * password;
	gchar * domain;
	gboolean domain_required;
};

GType rdp_server_get_type (void);
Server * rdp_server_new_from_keyfile (GKeyFile * keyfile, const gchar * name);
Server * rdp_server_new_from_json (JsonObject * object);

G_END_DECLS

#endif
