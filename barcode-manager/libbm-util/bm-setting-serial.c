/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/*
 * Dan Williams <dcbw@redhat.com>
 * Tambet Ingo <tambet@gmail.com>
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
 * (C) Copyright 2007 - 2011 Red Hat, Inc.
 * (C) Copyright 2007 - 2008 Novell, Inc.
 */

#include <string.h>

#include "bm-setting-serial.h"

GQuark
bm_setting_serial_error_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("bm-setting-serial-error-quark");
	return quark;
}

/* This should really be standard. */
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
bm_setting_serial_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			/* Unknown error. */
			ENUM_ENTRY (BM_SETTING_SERIAL_ERROR_UNKNOWN, "UnknownError"),
			/* The specified property was invalid. */
			ENUM_ENTRY (BM_SETTING_SERIAL_ERROR_INVALID_PROPERTY, "InvalidProperty"),
			/* The specified property was missing and is required. */
			ENUM_ENTRY (BM_SETTING_SERIAL_ERROR_MISSING_PROPERTY, "MissingProperty"),
			/* The required PPP setting is missing (DEPRECATED) */
			ENUM_ENTRY (BM_SETTING_SERIAL_ERROR_MISSING_PPP_SETTING, "MissingPPPSetting"),
			{ 0, 0, 0 }
		};
		etype = g_enum_register_static ("BMSettingSerialError", values);
	}
	return etype;
}


G_DEFINE_TYPE (BMSettingSerial, bm_setting_serial, BM_TYPE_SETTING)

#define BM_SETTING_SERIAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BM_TYPE_SETTING_SERIAL, BMSettingSerialPrivate))

typedef struct {
	guint baud;
	guint bits;
	char parity;
	guint stopbits;
	guint64 send_delay;
} BMSettingSerialPrivate;


enum {
	PROP_0,
	PROP_BAUD,
	PROP_BITS,
	PROP_PARITY,
	PROP_STOPBITS,
	PROP_SEND_DELAY,

	LAST_PROP
};

BMSetting *
bm_setting_serial_new (void)
{
	return (BMSetting *) g_object_new (BM_TYPE_SETTING_SERIAL, NULL);
}

guint
bm_setting_serial_get_baud (BMSettingSerial *setting)
{
	g_return_val_if_fail (BM_IS_SETTING_SERIAL (setting), 0);

	return BM_SETTING_SERIAL_GET_PRIVATE (setting)->baud;
}

guint
bm_setting_serial_get_bits (BMSettingSerial *setting)
{
	g_return_val_if_fail (BM_IS_SETTING_SERIAL (setting), 0);

	return BM_SETTING_SERIAL_GET_PRIVATE (setting)->bits;
}

char
bm_setting_serial_get_parity (BMSettingSerial *setting)
{
	g_return_val_if_fail (BM_IS_SETTING_SERIAL (setting), 0);

	return BM_SETTING_SERIAL_GET_PRIVATE (setting)->parity;
}

guint
bm_setting_serial_get_stopbits (BMSettingSerial *setting)
{
	g_return_val_if_fail (BM_IS_SETTING_SERIAL (setting), 0);

	return BM_SETTING_SERIAL_GET_PRIVATE (setting)->stopbits;
}

guint64
bm_setting_serial_get_send_delay (BMSettingSerial *setting)
{
	g_return_val_if_fail (BM_IS_SETTING_SERIAL (setting), 0);

	return BM_SETTING_SERIAL_GET_PRIVATE (setting)->send_delay;
}

static gboolean
verify (BMSetting *setting, GSList *all_settings, GError **error)
{
	return TRUE;
}

static void
bm_setting_serial_init (BMSettingSerial *setting)
{
	g_object_set (setting, BM_SETTING_NAME, BM_SETTING_SERIAL_SETTING_NAME, NULL);
}

static void
set_property (GObject *object, guint prop_id,
		    const GValue *value, GParamSpec *pspec)
{
	BMSettingSerialPrivate *priv = BM_SETTING_SERIAL_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_BAUD:
		priv->baud = g_value_get_uint (value);
		break;
	case PROP_BITS:
		priv->bits = g_value_get_uint (value);
		break;
	case PROP_PARITY:
		priv->parity = g_value_get_char (value);
		break;
	case PROP_STOPBITS:
		priv->stopbits = g_value_get_uint (value);
		break;
	case PROP_SEND_DELAY:
		priv->send_delay = g_value_get_uint64 (value);
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
	BMSettingSerial *setting = BM_SETTING_SERIAL (object);

	switch (prop_id) {
	case PROP_BAUD:
		g_value_set_uint (value, bm_setting_serial_get_baud (setting));
		break;
	case PROP_BITS:
		g_value_set_uint (value, bm_setting_serial_get_bits (setting));
		break;
	case PROP_PARITY:
		g_value_set_char (value, bm_setting_serial_get_parity (setting));
		break;
	case PROP_STOPBITS:
		g_value_set_uint (value, bm_setting_serial_get_stopbits (setting));
		break;
	case PROP_SEND_DELAY:
		g_value_set_uint64 (value, bm_setting_serial_get_send_delay (setting));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
bm_setting_serial_class_init (BMSettingSerialClass *setting_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (setting_class);
	BMSettingClass *parent_class = BM_SETTING_CLASS (setting_class);

	g_type_class_add_private (setting_class, sizeof (BMSettingSerialPrivate));

	/* virtual methods */
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	parent_class->verify       = verify;

	/* Properties */

	/**
	 * BMSettingSerial:baud:
	 *
	 * Speed to use for communication over the serial port.  Note that this value
	 * usually has no effect for mobile broadband modems as they generally
	 * ignore speed settings and use the highest available speed.
	 **/
	g_object_class_install_property
		(object_class, PROP_BAUD,
		 g_param_spec_uint (BM_SETTING_SERIAL_BAUD,
						"Baud",
						"Speed to use for communication over the serial port.  "
						"Note that this value usually has no effect for mobile "
						"broadband modems as they generally ignore speed "
						"settings and use the highest available speed.",
						0, G_MAXUINT, 57600,
						G_PARAM_READWRITE | G_PARAM_CONSTRUCT | BM_SETTING_PARAM_SERIALIZE));

	/**
	 * BMSettingSerial:bits:
	 *
	 * Byte-width of the serial communication.
	 **/
	g_object_class_install_property
		(object_class, PROP_BITS,
		 g_param_spec_uint (BM_SETTING_SERIAL_BITS,
						"Bits",
						"Byte-width of the serial communication.  The 8 in "
						"'8n1' for example.",
						5, 8, 8,
						G_PARAM_READWRITE | G_PARAM_CONSTRUCT | BM_SETTING_PARAM_SERIALIZE));

	/**
	 * BMSettingSerial:parity:
	 *
	 * Parity setting of the serial port.  Either 'E' for even parity, 'o' for
	 * odd parity, or 'n' for no parity.
	 **/
	g_object_class_install_property
		(object_class, PROP_PARITY,
		 g_param_spec_char (BM_SETTING_SERIAL_PARITY,
						"Parity",
						"Parity setting of the serial port.  Either 'E' for even "
						"parity, 'o' for odd parity, or 'n' for no parity.",
						'E', 'o', 'n',
						G_PARAM_READWRITE | G_PARAM_CONSTRUCT | BM_SETTING_PARAM_SERIALIZE));

	/**
	 * BMSettingSerial:stopbits:
	 *
	 * Number of stop bits for communication on the serial port.  Either 1 or 2.
	 * The 1 in '8n1' for example.
	 **/
	g_object_class_install_property
		(object_class, PROP_STOPBITS,
		 g_param_spec_uint (BM_SETTING_SERIAL_STOPBITS,
						"Stopbits",
						"Number of stop bits for communication on the serial "
						"port.  Either 1 or 2.  The 1 in '8n1' for example.",
						1, 2, 1,
						G_PARAM_READWRITE | G_PARAM_CONSTRUCT | BM_SETTING_PARAM_SERIALIZE));

	/**
	 * BMSettingSerial:send-delay:
	 *
	 * Time to delay between each byte sent to the modem, in microseconds.
	 **/
	g_object_class_install_property
		(object_class, PROP_SEND_DELAY,
		 g_param_spec_uint64 (BM_SETTING_SERIAL_SEND_DELAY,
						  "SendDelay",
						  "Time to delay between each byte sent to the modem, "
						  "in microseconds.",
						  0, G_MAXUINT64, 0,
						  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | BM_SETTING_PARAM_SERIALIZE));
}