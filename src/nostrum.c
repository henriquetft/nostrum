/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Main file of Relay Nostrum                                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-relay.h"
#include "nostrum-config.h"
#include "nostrum-version.h"
#include <glib-unix.h>
#include <glib.h>

#define G_LOG_DOMAIN "nostrum"


static gboolean
on_signal_quit (gpointer data)
{
        GMainLoop *loop = data;
        g_main_loop_quit (loop);
        return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
        (void)argc;
        (void)argv;

        g_message ("    _   __           __");
        g_message ("   / | / /___  _____/ /________  ______ ___");
        g_message ("  /  |/ / __ \\/ ___/ __/ ___/ / / / __ `__ \\");
        g_message (" / /|  / /_/ (__  ) /_/ /  / /_/ / / / / / /");
        g_message ("/_/ |_/\\____/____/\\__/_/   \\__,_/_/ /_/ /_/");
        g_message ("                          v%s", NOSTRUM_VERSION);
        g_message("");
        g_message ("Starting nostrum v%s ...", NOSTRUM_VERSION);

        // LOAD CONFIGURATION --------------------------------------------------
        struct NostrumRelayConfig cfg;
        nostrum_relay_config_init (&cfg);

        GError *error = NULL;
        gboolean result = nostrum_relay_config_load (&cfg, &error);
        if (!result) {
                g_critical ("Failed to load config file: %s", error->message);
                g_clear_error (&error);
                return EXIT_FAILURE;
        }
        g_autofree gchar *cfg_str = nostrum_relay_config_to_string (&cfg);
        g_debug ("Configuration:\n%s\n", cfg_str);

        // CREATE RELAY --------------------------------------------------------
        g_autoptr (NostrumRelay) relay = nostrum_relay_new (&cfg);
        g_autoptr (GError) err = NULL;

        if (!nostrum_relay_listen (relay, &err)) {
                g_critical ("Failed to start relay: %s", err->message);
                return EXIT_FAILURE;
        }

        // RUN MAIN LOOP AND SETUP SIGNALS -------------------------------------
        g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

        g_unix_signal_add (SIGINT, on_signal_quit, loop);
        g_unix_signal_add (SIGTERM, on_signal_quit, loop);

        g_main_loop_run (loop);

        g_message ("Quitting nostrum ...");

        return EXIT_SUCCESS;
}
