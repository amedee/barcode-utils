/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* BarcodeManager Connection editor -- Connection editor for BarcodeManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * (C) Copyright 2008 - 2010 Red Hat, Inc.
 */

#include <config.h>

#include <net/ethernet.h>
#include <netinet/ether.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#include <bm-setting-connection.h>
#include <bm-utils.h>
#include <bm-settings-connection-interface.h>

#include "ce-page.h"
#include "bma-marshal.h"
#include "utils.h"

G_DEFINE_ABSTRACT_TYPE (CEPage, ce_page, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_INITIALIZED,
	PROP_PARENT_WINDOW,

	LAST_PROP
};

enum {
	CHANGED,
	INITIALIZED,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

gint
ce_spin_output_with_default (GtkSpinButton *spin, gpointer user_data)
{
	int defvalue = GPOINTER_TO_INT (user_data);
	int val;
	gchar *buf = NULL;

	val = gtk_spin_button_get_value_as_int (spin);
	if (val == defvalue)
		buf = g_strdup (_("automatic"));
	else
		buf = g_strdup_printf ("%d", val);

	if (strcmp (buf, gtk_entry_get_text (GTK_ENTRY (spin))))
		gtk_entry_set_text (GTK_ENTRY (spin), buf);

	g_free (buf);
	return TRUE;
}

int
ce_get_property_default (BMSetting *setting, const char *property_name)
{
	GParamSpec *spec;
	GValue value = { 0, };

	spec = g_object_class_find_property (G_OBJECT_GET_CLASS (setting), property_name);
	g_return_val_if_fail (spec != NULL, -1);

	g_value_init (&value, spec->value_type);
	g_param_value_set_default (spec, &value);

	if (G_VALUE_HOLDS_CHAR (&value))
		return (int) g_value_get_char (&value);
	else if (G_VALUE_HOLDS_INT (&value))
		return g_value_get_int (&value);
	else if (G_VALUE_HOLDS_INT64 (&value))
		return (int) g_value_get_int64 (&value);
	else if (G_VALUE_HOLDS_LONG (&value))
		return (int) g_value_get_long (&value);
	else if (G_VALUE_HOLDS_UINT (&value))
		return (int) g_value_get_uint (&value);
	else if (G_VALUE_HOLDS_UINT64 (&value))
		return (int) g_value_get_uint64 (&value);
	else if (G_VALUE_HOLDS_ULONG (&value))
		return (int) g_value_get_ulong (&value);
	else if (G_VALUE_HOLDS_UCHAR (&value))
		return (int) g_value_get_uchar (&value);
	g_return_val_if_fail (FALSE, 0);
	return 0;
}

gboolean
ce_page_validate (CEPage *self, BMConnection *connection, GError **error)
{
	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);
	g_return_val_if_fail (BM_IS_CONNECTION (connection), FALSE);

	if (CE_PAGE_GET_CLASS (self)->validate)
		return CE_PAGE_GET_CLASS (self)->validate (self, connection, error);

	return TRUE;
}

static void
emit_initialized (CEPage *self, GError *error)
{
	self->initialized = TRUE;
	g_signal_emit (self, signals[INITIALIZED], 0, error);
}

void
ce_page_complete_init (CEPage *self,
                       const char *setting_name,
                       GHashTable *secrets,
                       GError *error)
{
	GError *update_error = NULL;
	GHashTable *setting_hash;

	g_return_if_fail (self != NULL);
	g_return_if_fail (CE_IS_PAGE (self));

	/* Ignore missing settings errors */
	if (error && !dbus_g_error_has_name (error, "org.freedesktop.BarcodeManagerSettings.InvalidSetting")) {
		emit_initialized (self, error);
		return;
	} else if (!setting_name || !secrets) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}

	g_assert (setting_name);
	g_assert (secrets);

	setting_hash = g_hash_table_lookup (secrets, setting_name);
	if (!setting_hash) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}

	/* Update the connection with the new secrets */
	if (bm_connection_update_secrets (self->connection,
	                                  setting_name,
	                                  setting_hash,
	                                  &update_error)) {
		/* Success */
		emit_initialized (self, NULL);
		return;
	}

	if (!update_error) {
		g_set_error_literal (&update_error, 0, 0,
		                     _("Failed to update connection secrets due to an unknown error."));
	}

	emit_initialized (self, update_error);
	g_clear_error (&update_error);
}

static void
ce_page_init (CEPage *self)
{
	self->builder = gtk_builder_new ();
}

static void
dispose (GObject *object)
{
	CEPage *self = CE_PAGE (object);

	if (self->disposed)
		return;

	self->disposed = TRUE;

	if (self->page)
		g_object_unref (self->page);

	if (self->builder)
		g_object_unref (self->builder);

	if (self->proxy)
		g_object_unref (self->proxy);

	if (self->connection)
		g_object_unref (self->connection);

	G_OBJECT_CLASS (ce_page_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	CEPage *self = CE_PAGE (object);

	g_free (self->title);

	G_OBJECT_CLASS (ce_page_parent_class)->finalize (object);
}

GtkWidget *
ce_page_get_page (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), NULL);

	return self->page;
}

const char *
ce_page_get_title (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), NULL);

	return self->title;
}

gboolean
ce_page_get_initialized (CEPage *self)
{
	g_return_val_if_fail (CE_IS_PAGE (self), FALSE);

	return self->initialized;
}

void
ce_page_changed (CEPage *self)
{
	g_return_if_fail (CE_IS_PAGE (self));

	g_signal_emit (self, signals[CHANGED], 0);
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	CEPage *self = CE_PAGE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, self->connection);
		break;
	case PROP_INITIALIZED:
		g_value_set_boolean (value, self->initialized);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	CEPage *self = CE_PAGE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		if (self->connection)
			g_object_unref (self->connection);
		self->connection = g_value_dup_object (value);
		break;
	case PROP_PARENT_WINDOW:
		self->parent_window = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ce_page_class_init (CEPageClass *page_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (page_class);

	/* virtual methods */
	object_class->dispose      = dispose;
	object_class->finalize     = finalize;
	object_class->get_property = get_property;
	object_class->set_property = set_property;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_CONNECTION,
		 g_param_spec_object (CE_PAGE_CONNECTION,
		                      "Connection",
		                      "Connection",
		                      BM_TYPE_CONNECTION,
		                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_INITIALIZED,
		 g_param_spec_boolean (CE_PAGE_INITIALIZED,
		                       "Initialized",
		                       "Initialized",
		                       FALSE,
		                       G_PARAM_READABLE));

	g_object_class_install_property
		(object_class, PROP_PARENT_WINDOW,
		 g_param_spec_pointer (CE_PAGE_PARENT_WINDOW,
		                       "Parent window",
		                       "Parent window",
		                       G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	/* Signals */
	signals[CHANGED] = 
		g_signal_new ("changed",
	                      G_OBJECT_CLASS_TYPE (object_class),
	                      G_SIGNAL_RUN_FIRST,
	                      G_STRUCT_OFFSET (CEPageClass, changed),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__VOID,
	                      G_TYPE_NONE, 0);

	signals[INITIALIZED] = 
		g_signal_new ("initialized",
	                      G_OBJECT_CLASS_TYPE (object_class),
	                      G_SIGNAL_RUN_FIRST,
	                      G_STRUCT_OFFSET (CEPageClass, initialized),
	                      NULL, NULL,
	                      g_cclosure_marshal_VOID__POINTER,
	                      G_TYPE_NONE, 1, G_TYPE_POINTER);
}


BMConnection *
ce_page_new_connection (const char *format,
                        const char *ctype,
                        gboolean autoconnect,
                        PageGetConnectionsFunc get_connections_func,
                        gpointer user_data)
{
	BMConnection *connection;
	BMSettingConnection *s_con;
	char *uuid, *id;
	GSList *connections;

	connection = bm_connection_new ();
	bm_connection_set_scope (connection, BM_CONNECTION_SCOPE_USER);

	s_con = BM_SETTING_CONNECTION (bm_setting_connection_new ());
	bm_connection_add_setting (connection, BM_SETTING (s_con));

	// FIXME uuid = bm_utils_uuid_generate ();
	uuid = NULL;

	connections = (*get_connections_func) (user_data);
	id = utils_next_available_name (connections, format);
	g_slist_free (connections);

	g_object_set (s_con,
	              BM_SETTING_CONNECTION_UUID, uuid,
	              BM_SETTING_CONNECTION_ID, id,
	              BM_SETTING_CONNECTION_TYPE, ctype,
	              BM_SETTING_CONNECTION_AUTOCONNECT, autoconnect,
	              NULL);

	g_free (uuid);
	g_free (id);

	return connection;
}

CEPage *
ce_page_new (GType page_type,
             BMConnection *connection,
             GtkWindow *parent_window,
             const char *ui_file,
             const char *widget_name,
             const char *title)
{
	CEPage *self;
	GError *error = NULL;

	g_return_val_if_fail (title != NULL, NULL);
	if (ui_file)
		g_return_val_if_fail (widget_name != NULL, NULL);

	self = CE_PAGE (g_object_new (page_type,
	                              CE_PAGE_CONNECTION, connection,
	                              CE_PAGE_PARENT_WINDOW, parent_window,
	                              NULL));
	self->title = g_strdup (title);
	if (ui_file) {
		if (!gtk_builder_add_from_file (self->builder, ui_file, &error)) {
			g_warning ("Couldn't load builder file: %s", error->message);
			g_error_free (error);
			g_object_unref (self);
			return NULL;
		}

		self->page = GTK_WIDGET (gtk_builder_get_object (self->builder, widget_name));
		if (!self->page) {
			g_warning ("Couldn't load page widget '%s' from %s", widget_name, ui_file);
			g_object_unref (self);
			return NULL;
		}
		g_object_ref_sink (self->page);
	}
	return self;
}

