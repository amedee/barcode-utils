/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* BarcodeManager -- Network link manager
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
 * (C) Copyright 2005 - 2010 Red Hat, Inc.
 */

#ifndef GCONF_HELPERS_H
#define GCONF_HELPERS_H

#include <gconf/gconf-client.h>
#include <glib.h>
#include <bm-connection.h>

#include "bma-gconf-connection.h"

#define GCONF_PATH_CONNECTIONS "/system/networking/connections"

/* The stamp is a mechanism for determining which applet version last
 * updated GConf for various GConf update tasks in newer applet versions.
 */
#define APPLET_CURRENT_STAMP 1
#define APPLET_PREFS_STAMP "/apps/bm-applet/stamp"

#define IGNORE_CA_CERT_TAG "ignore-ca-cert"
#define IGNORE_PHASE2_CA_CERT_TAG "ignore-phase2-ca-cert"

#define KEYRING_UUID_TAG "connection-uuid"
#define KEYRING_SN_TAG "setting-name"
#define KEYRING_SK_TAG "setting-key"

gboolean
bm_gconf_get_int_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					int *value);

gboolean
bm_gconf_get_float_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					gfloat *value);

gboolean
bm_gconf_get_string_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					char **value);

gboolean
bm_gconf_get_bool_helper (GConfClient *client,
					const char *path,
					const char *key,
					const char *setting,
					gboolean *value);

gboolean
bm_gconf_get_stringlist_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GSList **value);

gboolean
bm_gconf_get_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray **value);

gboolean
bm_gconf_get_bytearray_helper (GConfClient *client,
			       const char *path,
			       const char *key,
			       const char *setting,
			       GByteArray **value);

gboolean
bm_gconf_get_uint_array_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GArray **value);


#if UNUSED
gboolean
bm_gconf_get_valuehash_helper (GConfClient *client,
			       const char *path,
			       const char *setting,
			       GHashTable **value);
#endif

gboolean
bm_gconf_get_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable **value);

gboolean
bm_gconf_get_ip4_helper (GConfClient *client,
						  const char *path,
						  const char *key,
						  const char *setting,
						  guint32 tuple_len,
						  GPtrArray **value);

gboolean
bm_gconf_get_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray **value);

gboolean
bm_gconf_get_ip6addr_array_helper (GConfClient *client,
								   const char *path,
								   const char *key,
								   const char *setting,
								   GPtrArray **value);

gboolean
bm_gconf_get_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray **value);

/* Setters */

gboolean
bm_gconf_set_int_helper (GConfClient *client,
                         const char *path,
                         const char *key,
                         const char *setting,
                         int value);

gboolean
bm_gconf_set_float_helper (GConfClient *client,
                           const char *path,
                           const char *key,
                           const char *setting,
                           gfloat value);

gboolean
bm_gconf_set_string_helper (GConfClient *client,
                            const char *path,
                            const char *key,
                            const char *setting,
                            const char *value);

gboolean
bm_gconf_set_bool_helper (GConfClient *client,
                          const char *path,
                          const char *key,
                          const char *setting,
                          gboolean value);

gboolean
bm_gconf_set_stringlist_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GSList *value);

gboolean
bm_gconf_set_stringarray_helper (GConfClient *client,
                                 const char *path,
                                 const char *key,
                                 const char *setting,
                                 GPtrArray *value);

gboolean
bm_gconf_set_bytearray_helper (GConfClient *client,
                               const char *path,
                               const char *key,
                               const char *setting,
                               GByteArray *value);

gboolean
bm_gconf_set_uint_array_helper (GConfClient *client,
				const char *path,
				const char *key,
				const char *setting,
				GArray *value);

#if UNUSED
gboolean
bm_gconf_set_valuehash_helper (GConfClient *client,
			       const char *path,
			       const char *setting,
			       GHashTable *value);
#endif

gboolean
bm_gconf_set_stringhash_helper (GConfClient *client,
                                const char *path,
                                const char *key,
                                const char *setting,
                                GHashTable *value);

gboolean
bm_gconf_set_ip4_helper (GConfClient *client,
					  const char *path,
					  const char *key,
					  const char *setting,
					  guint32 tuple_len,
					  GPtrArray *value);

gboolean
bm_gconf_set_ip6dns_array_helper (GConfClient *client,
								  const char *path,
								  const char *key,
								  const char *setting,
								  GPtrArray *value);

gboolean
bm_gconf_set_ip6addr_array_helper (GConfClient *client,
								   const char *path,
								   const char *key,
								   const char *setting,
								   GPtrArray *value);

gboolean
bm_gconf_set_ip6route_array_helper (GConfClient *client,
									const char *path,
									const char *key,
									const char *setting,
									GPtrArray *value);

gboolean
bm_gconf_key_is_set (GConfClient *client,
                     const char *path,
                     const char *key,
                     const char *setting);

GSList *
bm_gconf_get_all_connections (GConfClient *client);

BMConnection *
bm_gconf_read_connection (GConfClient *client,
                          const char *dir);

void
bm_gconf_write_connection (BMConnection *connection,
                           GConfClient *client,
                           const char *dir,
                           gboolean ignore_secrets);

void
bm_gconf_add_keyring_item (const char *connection_uuid,
                           const char *connection_name,
                           const char *setting_name,
                           const char *setting_key,
                           const char *secret);

typedef void (*PreKeyringCallback) (gpointer user_data);
void bm_gconf_set_pre_keyring_callback (PreKeyringCallback func, gpointer user_data);
void pre_keyring_callback (void);

gboolean bm_gconf_get_ignore_ca_cert (const char *uuid, gboolean phase2);
void bm_gconf_set_ignore_ca_cert (const char *uuid, gboolean phase2, gboolean ignore);

gboolean bm_gconf_get_8021x_password_always_ask (const char *uuid);
void bm_gconf_set_8021x_password_always_ask (const char *uuid, gboolean always_ask);

#endif	/* GCONF_HELPERS_H */

