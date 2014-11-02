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

#ifndef __UCCS_SERVER_H__
#define __UCCS_SERVER_H__

#include <glib-object.h>
#include <libnm-glib/nm-client.h>
#include <libsoup/soup.h>
#include "server.h"

G_BEGIN_DECLS

#define UCCS_SERVER_TYPE            (uccs_server_get_type ())
#define UCCS_SERVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UCCS_SERVER_TYPE, UccsServer))
#define UCCS_SERVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UCCS_SERVER_TYPE, UccsServerClass))
#define IS_UCCS_SERVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UCCS_SERVER_TYPE))
#define IS_UCCS_SERVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UCCS_SERVER_TYPE))
#define UCCS_SERVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UCCS_SERVER_TYPE, UccsServerClass))

typedef struct _UccsServer      UccsServer;
typedef struct _UccsServerClass UccsServerClass;

struct _UccsServerClass {
	ServerClass parent_class;
};

struct _UccsServer {
	Server parent;

	gchar * exec;

	gchar * username;
	gchar * password;

	GHashTable * lovers;

	GList * subservers;

	GList * json_waiters;
	guint json_watch;
	GPid json_pid;

	GInputStream * json_stream;
	GOutputStream * pass_stream;

	NMState min_network;
	NMState last_network;
	NMClient * nm_client;
	gulong nm_signal;

	gboolean verify_server;
	gboolean verified_server;
	SoupSession * session;
};

GType uccs_server_get_type (void);
Server * uccs_server_new_from_keyfile (GKeyFile * keyfile, const gchar * name);
void uccs_server_unlock (UccsServer * server, const gchar * address, const gchar * username, const gchar * password, gboolean allowcache, void (*callback) (UccsServer * server, gboolean unlocked, gpointer user_data), gpointer user_data);
GVariant * uccs_server_get_servers (UccsServer * server, const gchar * address);
const gchar *uccs_server_set_exec (UccsServer * server, const gchar * exec);

G_END_DECLS

#endif
