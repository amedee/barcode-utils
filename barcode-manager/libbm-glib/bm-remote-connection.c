/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * libbm_glib -- Access barcode scanner hardware & information from glib applications
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2011 Jakob Flierl
 */

#include <string.h>

#include <BarcodeManager.h>
#include <bm-utils.h>
#include <bm-setting-connection.h>
#include "bm-remote-connection.h"
#include "bm-remote-connection-private.h"
#include "bm-dbus-glib-types.h"
#include "bm-exported-connection-bindings.h"
#include "bm-settings-connection-interface.h"

#define BM_REMOTE_CONNECTION_BUS "bus"

static void settings_connection_interface_init (BMSettingsConnectionInterface *klass);

G_DEFINE_TYPE_EXTENDED (BMRemoteConnection, bm_remote_connection, BM_TYPE_CONNECTION, 0,
                        G_IMPLEMENT_INTERFACE (BM_TYPE_SETTINGS_CONNECTION_INTERFACE, settings_connection_interface_init))

enum {
	PROP_0,
	PROP_BUS,
	PROP_INIT_RESULT,

	LAST_PROP
};


typedef struct {
	BMRemoteConnection *self;
	DBusGProxy *proxy;
	DBusGProxyCall *call;
	GFunc callback;
	gpointer user_data;
} RemoteCall;

typedef struct {
	DBusGConnection *bus;
	DBusGProxy *proxy;
	DBusGProxy *secrets_proxy;
	GSList *calls;

	BMRemoteConnectionInitResult init_result;
	gboolean disposed;
} BMRemoteConnectionPrivate;

#define BM_REMOTE_CONNECTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BM_TYPE_REMOTE_CONNECTION, BMRemoteConnectionPrivate))

/****************************************************************/

static void
remote_call_complete (BMRemoteConnection *self, RemoteCall *call)
{
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);

	priv->calls = g_slist_remove (priv->calls, call);
	/* Don't need to cancel it since this function should only be called from
	 * the dispose handler (where the proxy will be destroyed immediately after)
	 * or from the call's completion callback.
	 */
	memset (call, 0, sizeof (RemoteCall));
	g_free (call);
}

static void
update_cb (DBusGProxy *proxy, GError *error, gpointer user_data)
{
	RemoteCall *call = user_data;
	BMSettingsConnectionInterfaceUpdateFunc func = (BMSettingsConnectionInterfaceUpdateFunc) call->callback;

	(*func)(BM_SETTINGS_CONNECTION_INTERFACE (call->self), error, call->user_data);
	remote_call_complete (call->self, call);
}

static gboolean
update (BMSettingsConnectionInterface *connection,
        BMSettingsConnectionInterfaceUpdateFunc callback,
        gpointer user_data)
{
	BMRemoteConnection *self = BM_REMOTE_CONNECTION (connection);
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);
	GHashTable *settings = NULL;
	RemoteCall *call;

	call = g_malloc0 (sizeof (RemoteCall));
	call->self = self;
	call->callback = (GFunc) callback;
	call->user_data = user_data;
	call->proxy = priv->proxy;

	settings = bm_connection_to_hash (BM_CONNECTION (self));

	call->call = org_freedesktop_BarcodeManagerSettings_Connection_update_async (priv->proxy,
	                                                                             settings,
	                                                                             update_cb,
	                                                                             call);
	g_assert (call->call);
	priv->calls = g_slist_append (priv->calls, call);

	g_hash_table_destroy (settings);

	return TRUE;
}

static void
delete_cb (DBusGProxy *proxy, GError *error, gpointer user_data)
{
	RemoteCall *call = user_data;
	BMSettingsConnectionInterfaceDeleteFunc func = (BMSettingsConnectionInterfaceDeleteFunc) call->callback;

	(*func)(BM_SETTINGS_CONNECTION_INTERFACE (call->self), error, call->user_data);
	remote_call_complete (call->self, call);
}

static gboolean
do_delete (BMSettingsConnectionInterface *connection,
           BMSettingsConnectionInterfaceDeleteFunc callback,
           gpointer user_data)
{
	BMRemoteConnection *self = BM_REMOTE_CONNECTION (connection);
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);
	RemoteCall *call;

	call = g_malloc0 (sizeof (RemoteCall));
	call->self = self;
	call->callback = (GFunc) callback;
	call->user_data = user_data;
	call->proxy = priv->proxy;

	call->call = org_freedesktop_BarcodeManagerSettings_Connection_delete_async (priv->proxy,
	                                                                             delete_cb,
	                                                                             call);
	g_assert (call->call);
	priv->calls = g_slist_append (priv->calls, call);

	return TRUE;
}

static void
get_secrets_cb (DBusGProxy *proxy, GHashTable *secrets, GError *error, gpointer user_data)
{
	RemoteCall *call = user_data;
	BMSettingsConnectionInterfaceGetSecretsFunc func = (BMSettingsConnectionInterfaceGetSecretsFunc) call->callback;

	(*func)(BM_SETTINGS_CONNECTION_INTERFACE (call->self), error ? NULL : secrets, error, call->user_data);
	remote_call_complete (call->self, call);
}

static gboolean
get_secrets (BMSettingsConnectionInterface *connection,
             const char *setting_name,
             const char **hints,
             gboolean request_new,
             BMSettingsConnectionInterfaceGetSecretsFunc callback,
             gpointer user_data)
{
	BMRemoteConnection *self = BM_REMOTE_CONNECTION (connection);
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);
	RemoteCall *call;

	call = g_malloc0 (sizeof (RemoteCall));
	call->self = self;
	call->callback = (GFunc) callback;
	call->user_data = user_data;
	call->proxy = priv->secrets_proxy;

	call->call = org_freedesktop_BarcodeManagerSettings_Connection_Secrets_get_secrets_async (priv->secrets_proxy,
	                                                                                          setting_name,
	                                                                                          hints,
	                                                                                          request_new,
	                                                                                          get_secrets_cb,
	                                                                                          call);
	g_assert (call->call);
	priv->calls = g_slist_append (priv->calls, call);

	return TRUE;
}

/****************************************************************/

static gboolean
replace_settings (BMRemoteConnection *self, GHashTable *new_settings)
{
	GError *error = NULL;

	if (!bm_connection_replace_settings (BM_CONNECTION (self), new_settings, &error)) {
		g_warning ("%s: error updating %s connection %s settings: (%d) %s",
		           __func__,
		           (bm_connection_get_scope (BM_CONNECTION (self)) == BM_CONNECTION_SCOPE_USER) ? "user" : "system",
		           bm_connection_get_path (BM_CONNECTION (self)),
		           error ? error->code : -1,
		           (error && error->message) ? error->message : "(unknown)");
		g_clear_error (&error);
		return FALSE;
	}

	/* Emit update irregardless to let listeners figure out what to do with
	 * the connection; whether to delete / ignore it or not.
	 */
	bm_settings_connection_interface_emit_updated (BM_SETTINGS_CONNECTION_INTERFACE (self));
	return TRUE;
}

static void
get_settings_cb (DBusGProxy *proxy,
                 GHashTable *new_settings,
                 GError *error,
                 gpointer user_data)
{
	BMRemoteConnection *self = user_data;
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);

	if (error) {
		g_warning ("%s: error getting %s connection %s settings: (%d) %s",
		           __func__,
		           (bm_connection_get_scope (BM_CONNECTION (self)) == BM_CONNECTION_SCOPE_USER) ? "user" : "system",
		           bm_connection_get_path (BM_CONNECTION (self)),
		           error ? error->code : -1,
		           (error && error->message) ? error->message : "(unknown)");
		g_error_free (error);
		priv->init_result = BM_REMOTE_CONNECTION_INIT_RESULT_ERROR;
		g_object_notify (G_OBJECT (self), BM_REMOTE_CONNECTION_INIT_RESULT);
	} else {
		replace_settings (self, new_settings);
		g_hash_table_destroy (new_settings);
		priv->init_result = BM_REMOTE_CONNECTION_INIT_RESULT_SUCCESS;
		g_object_notify (G_OBJECT (self), BM_REMOTE_CONNECTION_INIT_RESULT);
	}
}

static void
updated_cb (DBusGProxy *proxy, GHashTable *settings, gpointer user_data)
{
	replace_settings (BM_REMOTE_CONNECTION (user_data), settings);
}

static void
removed_cb (DBusGProxy *proxy, gpointer user_data)
{
	g_signal_emit_by_name (G_OBJECT (user_data), "removed");
}

/****************************************************************/

static void
settings_connection_interface_init (BMSettingsConnectionInterface *klass)
{
	/* interface implementation */
	klass->update = update;
	klass->delete = do_delete;
	klass->get_secrets = get_secrets;
}

/**
 * bm_remote_connection_new:
 * @bus: a valid and connected D-Bus connection
 * @scope: the Connection scope (either user or system)
 * @path: the D-Bus path of the connection as exported by the settings service
 *  indicated by @scope
 *
 * Creates a new object representing the remote connection.
 *
 * Returns: the new remote connection object on success, or %NULL on failure
 **/
BMRemoteConnection *
bm_remote_connection_new (DBusGConnection *bus,
                          BMConnectionScope scope,
                          const char *path)
{
	g_return_val_if_fail (bus != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	return (BMRemoteConnection *) g_object_new (BM_TYPE_REMOTE_CONNECTION,
	                                            BM_REMOTE_CONNECTION_BUS, bus,
	                                            BM_CONNECTION_SCOPE, scope,
	                                            BM_CONNECTION_PATH, path,
	                                            NULL);
}

static GObject *
constructor (GType type,
             guint n_construct_params,
             GObjectConstructParam *construct_params)
{
	GObject *object;
	BMRemoteConnectionPrivate *priv;
	const char *service = BM_DBUS_SERVICE_USER_SETTINGS;

	object = G_OBJECT_CLASS (bm_remote_connection_parent_class)->constructor (type, n_construct_params, construct_params);
	if (!object)
		return NULL;

	priv = BM_REMOTE_CONNECTION_GET_PRIVATE (object);
	g_assert (priv->bus);
	g_assert (bm_connection_get_path (BM_CONNECTION (object)));

	if (bm_connection_get_scope (BM_CONNECTION (object)) == BM_CONNECTION_SCOPE_SYSTEM)
		service = BM_DBUS_SERVICE_SYSTEM_SETTINGS;

	priv->proxy = dbus_g_proxy_new_for_name (priv->bus,
	                                         service,
	                                         bm_connection_get_path (BM_CONNECTION (object)),
	                                         BM_DBUS_IFACE_SETTINGS_CONNECTION);
	g_assert (priv->proxy);
	dbus_g_proxy_set_default_timeout (priv->proxy, G_MAXINT);

	priv->secrets_proxy = dbus_g_proxy_new_for_name (priv->bus,
	                                                 service,
	                                                 bm_connection_get_path (BM_CONNECTION (object)),
	                                                 BM_DBUS_IFACE_SETTINGS_CONNECTION_SECRETS);
	g_assert (priv->secrets_proxy);
	dbus_g_proxy_set_default_timeout (priv->secrets_proxy, G_MAXINT);

	dbus_g_proxy_add_signal (priv->proxy, "Updated", DBUS_TYPE_G_MAP_OF_MAP_OF_VARIANT, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->proxy, "Updated", G_CALLBACK (updated_cb), object, NULL);

	dbus_g_proxy_add_signal (priv->proxy, "Removed", G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->proxy, "Removed", G_CALLBACK (removed_cb), object, NULL);

	org_freedesktop_BarcodeManagerSettings_Connection_get_settings_async (priv->proxy,
	                                                                      get_settings_cb,
	                                                                      object);
	return object;
}

static void
bm_remote_connection_init (BMRemoteConnection *self)
{
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_BUS:
		/* Construct only */
		priv->bus = dbus_g_connection_ref ((DBusGConnection *) g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
	BMRemoteConnection *self = BM_REMOTE_CONNECTION (object);
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (self);

	switch (prop_id) {
	case PROP_INIT_RESULT:
		g_value_set_uint (value, priv->init_result);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
dispose (GObject *object)
{
	BMRemoteConnection *self = BM_REMOTE_CONNECTION (object);
	BMRemoteConnectionPrivate *priv = BM_REMOTE_CONNECTION_GET_PRIVATE (object);

	if (!priv->disposed) {
		priv->disposed = TRUE;

		while (g_slist_length (priv->calls))
			remote_call_complete (self, priv->calls->data);

		g_object_unref (priv->proxy);
		g_object_unref (priv->secrets_proxy);
		dbus_g_connection_unref (priv->bus);
	}

	G_OBJECT_CLASS (bm_remote_connection_parent_class)->dispose (object);
}

static void
bm_remote_connection_class_init (BMRemoteConnectionClass *remote_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (remote_class);

	g_type_class_add_private (object_class, sizeof (BMRemoteConnectionPrivate));

	/* virtual methods */
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->dispose = dispose;
	object_class->constructor = constructor;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_BUS,
		 g_param_spec_boxed (BM_REMOTE_CONNECTION_BUS,
						 "DBusGConnection",
						 "DBusGConnection",
						 DBUS_TYPE_G_CONNECTION,
						 G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_INIT_RESULT,
		 g_param_spec_uint (BM_REMOTE_CONNECTION_INIT_RESULT,
		                    "Initialization result (PRIVATE)",
		                    "Initialization result (PRIVATE)",
		                    BM_REMOTE_CONNECTION_INIT_RESULT_UNKNOWN,
		                    BM_REMOTE_CONNECTION_INIT_RESULT_ERROR,
		                    BM_REMOTE_CONNECTION_INIT_RESULT_UNKNOWN,
		                    G_PARAM_READABLE));
}

