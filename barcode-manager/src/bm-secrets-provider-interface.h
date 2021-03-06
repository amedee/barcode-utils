/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* BarcodeManager -- Network link manager
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
 * Copyright (C) 2009 Red Hat, Inc.
 */

#ifndef BM_SECRETS_PROVIDER_INTERFACE_H
#define BM_SECRETS_PROVIDER_INTERFACE_H

#include <glib-object.h>
#include <bm-connection.h>

typedef enum {
	SECRETS_CALLER_NONE = 0,
	SECRETS_CALLER_ETHERNET,
	SECRETS_CALLER_WIFI,
	SECRETS_CALLER_MOBILE_BROADBAND,
	SECRETS_CALLER_PPP,
	SECRETS_CALLER_VPN
} RequestSecretsCaller;

#define BM_TYPE_SECRETS_PROVIDER_INTERFACE               (bm_secrets_provider_interface_get_type ())
#define BM_SECRETS_PROVIDER_INTERFACE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), BM_TYPE_SECRETS_PROVIDER_INTERFACE, NMSecretsProviderInterface))
#define BM_IS_SECRETS_PROVIDER_INTERFACE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BM_TYPE_SECRETS_PROVIDER_INTERFACE))
#define BM_SECRETS_PROVIDER_INTERFACE_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), BM_TYPE_SECRETS_PROVIDER_INTERFACE, NMSecretsProviderInterface))

typedef struct _NMSecretsProviderInterface NMSecretsProviderInterface;

struct _NMSecretsProviderInterface {
	GTypeInterface g_iface;

	/* Methods */
	void (*result)             (NMSecretsProviderInterface *self,
	                            const char *setting_name,
	                            RequestSecretsCaller caller,
	                            const GSList *updated,
	                            GError *error);

	gboolean (*update_setting) (NMSecretsProviderInterface *self,
	                            const char *setting_name,
	                            GHashTable *new);

	/* Signals */
	void (*manager_get_secrets)    (NMSecretsProviderInterface *self,
	                                BMConnection *connection,
	                                const char *setting_name,
	                                gboolean request_new,
	                                RequestSecretsCaller caller,
	                                const char *hint1,
	                                const char *hint2);

	void (*manager_cancel_secrets) (NMSecretsProviderInterface *self);
};

GType bm_secrets_provider_interface_get_type (void);

/* For callers */
gboolean bm_secrets_provider_interface_get_secrets    (NMSecretsProviderInterface *self,
                                                       BMConnection *connection,
                                                       const char *setting_name,
                                                       gboolean request_new,
                                                       RequestSecretsCaller caller,
                                                       const char *hint1,
                                                       const char *hint2);

void bm_secrets_provider_interface_cancel_get_secrets (NMSecretsProviderInterface *self);

/* For BMManager */
void bm_secrets_provider_interface_get_secrets_result (NMSecretsProviderInterface *self,
                                                       const char *setting_name,
                                                       RequestSecretsCaller caller,
                                                       GHashTable *settings,
                                                       GError *error);

#endif /* BM_SECRETS_PROVIDER_INTERFACE_H */

