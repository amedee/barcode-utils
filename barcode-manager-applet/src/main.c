/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* BarcodeManager Applet -- Display barcode scanners and allow user control
 *
 * Jakob Flierl <jakob.flierl@gmail.com>
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
 * (C) Copyright 2011 Jakob Flierl
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "applet.h"

static GMainLoop *loop = NULL;

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

static void
usage (const char *progname)
{
	char *foo;

	foo = g_path_get_basename (progname);
	fprintf (stdout, "%s %s\n\n%s\n%s\n\n",
	                 _("Usage:"),
	                 foo,
	                 _("This program is a component of BarcodeManager (http://github.com/koppi/barcode-utils )."),
	                 _("It is not intended for command-line interaction but instead runs in the GNOME desktop environment."));
	g_free (foo);
}

int main (int argc, char *argv[])
{
	BMApplet *applet;
	guint32 i;

	for (i = 1; i < argc; i++) {
		if (!strcmp (argv[i], "--help")) {
			usage (argv[0]);
			exit (0);
		}
	}

	bindtextdomain (GETTEXT_PACKAGE, BMALOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	gtk_init (&argc, &argv);
	textdomain (GETTEXT_PACKAGE);

	loop = g_main_loop_new (NULL, FALSE);

	applet = bm_applet_new (loop);
	if (applet == NULL)
		exit (1);

	setup_signals ();
	g_main_loop_run (loop);

	g_object_unref (G_OBJECT (applet));

	exit (0);
}

