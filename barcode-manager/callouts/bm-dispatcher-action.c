/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* BarcodeManager -- barcode scanner manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2011 Jakob Flierl
 */

#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <arpa/inet.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

#include <BarcodeManager.h>
#include "bm-glib-compat.h"
#include <libbm-util/bm-connection.h>
#include <libbm-util/bm-setting-connection.h>
#include <libbm-glib/bm-device.h>

#include "bm-dispatcher-action.h"

#define NMD_SCRIPT_DIR    SYSCONFDIR "/BarcodeManager/dispatcher.d"

static GMainLoop *loop = NULL;
static gboolean debug = FALSE;

static gboolean quit_timeout_cb (gpointer user_data);

typedef struct Handler Handler;
typedef struct HandlerClass HandlerClass;

GType handler_get_type (void);

struct Handler {
	GObject parent;
};

struct HandlerClass {
  GObjectClass parent;
};

#define HANDLER_TYPE              (handler_get_type ())
#define HANDLER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), HANDLER_TYPE, Handler))
#define HANDLER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HANDLER_TYPE, HandlerClass))
#define IS_HANDLER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), HANDLER_TYPE))
#define IS_HANDLER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HANDLER_TYPE))
#define HANDLER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HANDLER_TYPE, HandlerClass))

G_DEFINE_TYPE(Handler, handler, G_TYPE_OBJECT)

typedef struct {
	DBusGConnection *g_connection;
	DBusGProxy *bus_proxy;
	guint quit_timeout;
	gboolean persist;

	Handler *handler;
} Dispatcher;

static gboolean
bm_dispatcher_action (Handler *obj,
                      const char *action,
                      GHashTable *connection,
                      GHashTable *connection_props,
                      GHashTable *device_props,
                      GError **error);

#include "bm-dispatcher-glue.h"


static void
handler_init (Handler *h)
{
}

static void
handler_finalize (GObject *object)
{
	G_OBJECT_CLASS (handler_parent_class)->finalize (object);
}

static void
handler_class_init (HandlerClass *h_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (h_class);

  gobject_class->finalize = handler_finalize;
}

/*
 * bmd_permission_check
 *
 * Verify that the given script has the permissions we want.  Specifically,
 * ensure that the file is
 *	- A regular file.
 *	- Owned by root.
 *	- Not writable by the group or by other.
 *	- Not setuid.
 *	- Executable by the owner.
 *
 */
static inline gboolean
bmd_permission_check (struct stat *s, GError **error)
{
	g_return_val_if_fail (s != NULL, FALSE);
	g_return_val_if_fail (error != NULL, FALSE);
	g_return_val_if_fail (*error == NULL, FALSE);

	/* Only accept regular files */
	if (!S_ISREG (s->st_mode)) {
		g_set_error (error, 0, 0, "not a regular file.");
		return FALSE;
	}

	/* Only accept files owned by root */
	if (s->st_uid != 0) {
		g_set_error (error, 0, 0, "not owned by root.");
		return FALSE;
	}

	/* Only accept files not writable by group or other, and not SUID */
	if (s->st_mode & (S_IWGRP | S_IWOTH | S_ISUID)) {
		g_set_error (error, 0, 0, "writable by group or other, or set-UID.");
		return FALSE;
	}

	/* Only accept files executable by the owner */
	if (!(s->st_mode & S_IXUSR)) {
		g_set_error (error, 0, 0, "not executable by owner.");
		return FALSE;
	}

	return TRUE;
}


/*
 * bmd_is_valid_filename
 *
 * Verify that the given script is a valid file name. Specifically,
 * ensure that the file:
 *	- is not a editor backup file
 *	- is not a package management file
 *	- does not start with '.'
 */
static inline gboolean
bmd_is_valid_filename (const char *file_name)
{
	char *bad_suffixes[] = { "~", ".rpmsave", ".rpmorig", ".rpmnew", NULL };
	char *tmp;
	int i;

	if (file_name[0] == '.')
		return FALSE;
	for (i = 0; bad_suffixes[i]; i++) {
		if (g_str_has_suffix(file_name, bad_suffixes[i]))
			return FALSE;
	}
	tmp = g_strrstr(file_name, ".dpkg-");
	if (tmp && (tmp == strrchr(file_name,'.')))
		return FALSE;
	return TRUE;
}

static gint
sort_files (gconstpointer a, gconstpointer b)
{
	char *a_base = NULL, *b_base = NULL;
	int ret = 0;

	if (a && !b)
		return 1;
	if (!a && !b)
		return 0;
	if (!a && b)
		return -1;

	a_base = g_path_get_basename (a);
	b_base = g_path_get_basename (b);

	ret = strcmp (a_base, b_base);

	g_free (a_base);
	g_free (b_base);
	return ret;
}

static void
child_setup (gpointer user_data G_GNUC_UNUSED)
{
        /* We are in the child process at this point */
		/* Give child a different process group to ensure signal separation. */
        pid_t pid = getpid ();
        setpgid (pid, pid);
}

typedef struct {
	char **envp;
	guint32 i;
} EnvAddInfo;

static void
add_one_option_to_envp (gpointer key, gpointer value, gpointer user_data)
{
	EnvAddInfo *info = (EnvAddInfo *) user_data;
	char *ucased;

	ucased = g_ascii_strup (key, -1);
	info->envp[info->i++] = g_strdup_printf ("DHCP4_%s=%s", ucased, (char *) value);
	g_free (ucased);
}

static void
dispatch_scripts (const char *action,
                  const char *iface,
                  const char *parent_iface,
                  BMDeviceType type,
                  const char *uuid)
{
	GDir *dir;
	const char *filename;
	GSList *scripts = NULL, *iter;
	GError *error = NULL;
	char **envp = NULL;

	if (!(dir = g_dir_open (NMD_SCRIPT_DIR, 0, &error))) {
		g_warning ("g_dir_open() could not open '" NMD_SCRIPT_DIR "'.  '%s'",
		           error->message);
		g_error_free (error);
		return;
	}

	while ((filename = g_dir_read_name (dir))) {
		char *file_path;
		struct stat	s;
		GError *pc_error = NULL;
		int err;

		if (!bmd_is_valid_filename (filename))
			continue;

		file_path = g_build_filename (NMD_SCRIPT_DIR, filename, NULL);

		err = stat (file_path, &s);
		if (err) {
			g_warning ("Script '%s' could not be stated: %d", file_path, err);
			g_free (file_path);
			continue;
		}

		if (!bmd_permission_check (&s, &pc_error)) {
			g_warning ("Script '%s' could not be executed: %s", file_path, pc_error->message);
			g_error_free (pc_error);
			g_free (file_path);
		} else {
			/* success */
			scripts = g_slist_insert_sorted (scripts, file_path, sort_files);
		}
	}
	g_dir_close (dir);

	// FIXME invoke script here

	g_slist_foreach (scripts, (GFunc) g_free, NULL);
	g_slist_free (scripts);
}

static gboolean
bm_dispatcher_action (Handler *h,
                      const char *action,
                      GHashTable *connection_hash,
                      GHashTable *connection_props,
                      GHashTable *device_props,
                      GError **error)
{
	Dispatcher *d = g_object_get_data (G_OBJECT (h), "dispatcher");
	BMConnection *connection;
	char *iface = NULL, *parent_iface = NULL;
	const char *uuid = NULL;
	BMDeviceType type = BM_DEVICE_TYPE_UNKNOWN;
	BMDeviceState dev_state = BM_DEVICE_STATE_UNKNOWN;
	BMDevice *device = NULL;
	GValue *value;

	/* Back off the quit timeout */
	if (d->quit_timeout)
		g_source_remove (d->quit_timeout);
	if (!d->persist)
		d->quit_timeout = g_timeout_add_seconds (10, quit_timeout_cb, NULL);

	/* Hostname changes don't require a device nor contain a connection */
	if (!strcmp (action, "hostname"))
		goto dispatch;

	connection = bm_connection_new_from_hash (connection_hash, error);
	if (connection) {
		BMSettingConnection *s_con;

		s_con = BM_SETTING_CONNECTION (bm_connection_get_setting (connection, BM_TYPE_SETTING_CONNECTION));
		if (s_con)
			uuid = bm_setting_connection_get_uuid (s_con);
	} else {
		g_warning ("%s: Invalid connection: '%s' / '%s' invalid: %d",
		           __func__,
		           g_type_name (bm_connection_lookup_setting_type_by_quark ((*error)->domain)),
		           (*error)->message, (*error)->code);
		/* Don't fail on this error yet */
		g_error_free (*error);
		*error = NULL;
	}

	/* interface name */
	value = g_hash_table_lookup (device_props, BMD_DEVICE_PROPS_INTERFACE);
	if (!value || !G_VALUE_HOLDS_STRING (value)) {
		g_warning ("Missing or invalid required value " BMD_DEVICE_PROPS_INTERFACE "!");
		goto out;
	}
	iface = (char *) g_value_get_string (value);

	/* IP interface name */
	value = g_hash_table_lookup (device_props, BMD_DEVICE_PROPS_IP_INTERFACE);
	if (value) {
		if (!G_VALUE_HOLDS_STRING (value)) {
			g_warning ("Invalid required value " BMD_DEVICE_PROPS_IP_INTERFACE "!");
			goto out;
		}
		parent_iface = iface;
		iface = (char *) g_value_get_string (value);
	}

	/* Device type */
	value = g_hash_table_lookup (device_props, BMD_DEVICE_PROPS_TYPE);
	if (!value || !G_VALUE_HOLDS_UINT (value)) {
		g_warning ("Missing or invalid required value " BMD_DEVICE_PROPS_TYPE "!");
		goto out;
	}
	type = g_value_get_uint (value);

	/* Device state */
	value = g_hash_table_lookup (device_props, BMD_DEVICE_PROPS_STATE);
	if (!value || !G_VALUE_HOLDS_UINT (value)) {
		g_warning ("Missing or invalid required value " BMD_DEVICE_PROPS_STATE "!");
		goto out;
	}
	dev_state = g_value_get_uint (value);

	/* device itself */
	value = g_hash_table_lookup (device_props, BMD_DEVICE_PROPS_PATH);
	if (!value || (G_VALUE_TYPE (value) != DBUS_TYPE_G_OBJECT_PATH)) {
		g_warning ("Missing or invalid required value " BMD_DEVICE_PROPS_PATH "!");
		goto out;
	}
	device = BM_DEVICE (bm_device_new (d->g_connection, (const char *) g_value_get_boxed (value)));

dispatch:
	dispatch_scripts (action, iface, parent_iface, type, uuid);

	if (device)
		g_object_unref (device);

out:
	return TRUE;
}

static gboolean
start_dbus_service (Dispatcher *d)
{
	int request_name_result;
	GError *err = NULL;
	gboolean success = FALSE;

	if (!dbus_g_proxy_call (d->bus_proxy, "RequestName", &err,
							G_TYPE_STRING, BM_DISPATCHER_DBUS_SERVICE,
							G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
							G_TYPE_INVALID,
							G_TYPE_UINT, &request_name_result,
							G_TYPE_INVALID)) {
		g_warning ("Could not acquire the " BM_DISPATCHER_DBUS_SERVICE " service.\n"
		           "  Message: '%s'", err->message);
		g_error_free (err);
		goto out;
	}

	if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_warning ("Could not acquire the " BM_DISPATCHER_DBUS_SERVICE " service "
		           "as it is already taken.  Return: %d",
		           request_name_result);
		goto out;
	}
	success = TRUE;

out:
	return success;
}

static void
destroy_cb (DBusGProxy *proxy, gpointer user_data)
{
	g_warning ("Disconnected from the system bus, exiting.");
	g_main_loop_quit (loop);
}

static gboolean
dbus_init (Dispatcher *d)
{
	GError *err = NULL;
	DBusConnection *connection;
	
	dbus_connection_set_change_sigpipe (TRUE);

	d->g_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &err);
	if (!d->g_connection) {
		g_warning ("Could not get the system bus.  Make sure "
		           "the message bus daemon is running!  Message: %s",
		           err->message);
		g_error_free (err);
		return FALSE;
	}

	/* Clean up nicely if we get kicked off the bus */
	connection = dbus_g_connection_get_connection (d->g_connection);
	dbus_connection_set_exit_on_disconnect (connection, FALSE);

	d->bus_proxy = dbus_g_proxy_new_for_name (d->g_connection,
	                                          "org.freedesktop.DBus",
	                                          "/org/freedesktop/DBus",
	                                          "org.freedesktop.DBus");
	if (!d->bus_proxy) {
		g_warning ("Could not get the DBus object!");
		goto error;
	}

	g_signal_connect (d->bus_proxy, "destroy", G_CALLBACK (destroy_cb), NULL);

	return TRUE;

error:	
	return FALSE;
}

static void
log_handler (const gchar *log_domain,
             GLogLevelFlags log_level,
             const gchar *message,
             gpointer ignored)
{
	int syslog_priority;	

	switch (log_level) {
		case G_LOG_LEVEL_ERROR:
			syslog_priority = LOG_CRIT;
			break;

		case G_LOG_LEVEL_CRITICAL:
			syslog_priority = LOG_ERR;
			break;

		case G_LOG_LEVEL_WARNING:
			syslog_priority = LOG_WARNING;
			break;

		case G_LOG_LEVEL_MESSAGE:
			syslog_priority = LOG_NOTICE;
			break;

		case G_LOG_LEVEL_DEBUG:
			syslog_priority = LOG_DEBUG;
			break;

		case G_LOG_LEVEL_INFO:
		default:
			syslog_priority = LOG_INFO;
			break;
	}

	syslog (syslog_priority, "%s", message);
}


static void
logging_setup (void)
{
	openlog (G_LOG_DOMAIN, LOG_CONS, LOG_DAEMON);
	g_log_set_handler (G_LOG_DOMAIN, 
	                   G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
	                   log_handler,
	                   NULL);
}

static void
logging_shutdown (void)
{
	closelog ();
}

static void
signal_handler (int signo)
{
	if (signo == SIGINT || signo == SIGTERM) {
		g_message ("Caught signal %d, shutting down...", signo);
		g_main_loop_quit (loop);
	}
}

static void
setup_signals (void)
{
	struct sigaction action;
	sigset_t mask;

	sigemptyset (&mask);
	action.sa_handler = signal_handler;
	action.sa_mask = mask;
	action.sa_flags = 0;
	sigaction (SIGTERM,  &action, NULL);
	sigaction (SIGINT,  &action, NULL);
}

static gboolean
quit_timeout_cb (gpointer user_data)
{
	g_main_loop_quit (loop);
	return FALSE;
}

int
main (int argc, char **argv)
{
	Dispatcher *d = g_malloc0 (sizeof (Dispatcher));
	GOptionContext *opt_ctx;
	GError *error = NULL;
	gboolean persist = FALSE;

	GOptionEntry entries[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &debug, "Output to console rather than syslog", NULL },
		{ "persist", 0, 0, G_OPTION_ARG_NONE, &persist, "Don't quit after a short timeout", NULL },
		{ NULL }
	};

	opt_ctx = g_option_context_new (NULL);
	g_option_context_set_summary (opt_ctx, "Executes scripts upon actions by BarcodeManager.");
	g_option_context_add_main_entries (opt_ctx, entries, NULL);

	if (!g_option_context_parse (opt_ctx, &argc, &argv, &error)) {
		g_warning ("%s\n", error->message);
		g_error_free (error);
		g_free (d);
		return 1;
	}

	g_option_context_free (opt_ctx);

	g_type_init ();
	setup_signals ();

	if (!debug)
		logging_setup ();

	loop = g_main_loop_new (NULL, FALSE);

	if (!dbus_init (d))
		return -1;
	if (!start_dbus_service (d))
		return -1;

	d->persist = persist;
	d->handler = g_object_new (HANDLER_TYPE, NULL);
	if (!d->handler)
		return -1;
	g_object_set_data (G_OBJECT (d->handler), "dispatcher", d);

	dbus_g_object_type_install_info (HANDLER_TYPE, &dbus_glib_bm_dispatcher_object_info);
	dbus_g_connection_register_g_object (d->g_connection,
	                                     BM_DISPATCHER_DBUS_PATH,
	                                     G_OBJECT (d->handler));

	if (!persist)
		d->quit_timeout = g_timeout_add_seconds (10, quit_timeout_cb, NULL);

	g_main_loop_run (loop);

	g_object_unref (d->handler);
	dbus_g_connection_unref (d->g_connection);
	g_free (d);

	if (!debug)
		logging_shutdown ();

	return 0;
}

