/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Main file of Relay Nostrum                                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include <glib.h>
#include <glib-unix.h>
#include "nostrum-relay.h"
#include "nostrum-version.h"


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
        g_setenv("G_MESSAGES_DEBUG",
                 "nostrum nostrum-relay nostrum-storage",
                 TRUE);

        (void)argc;
        (void)argv;

        g_message ("Starting nostrum v%s ...", NOSTRUM_VERSION);
        // TODO read config from file
        struct NostrumRelayConfig cfg;
        nostrum_relay_config_init (&cfg);
        cfg.server_host       = "0.0.0.0";
        cfg.server_http_port  = 8080;
        cfg.server_https_port = 0;
        cfg.db_type           = "sqlite";
        cfg.db_path           = "nostrum_relay.db";

        cfg.info_name         = "Relay Nostrum";
        cfg.info_description  = "Nostrum relay";
        cfg.info_contact      = "admin@admin.me";
        
        
        
        g_autoptr (NostrumRelay) relay = nostrum_relay_new (&cfg);
        g_autoptr (GError) err = NULL;


        if (!nostrum_relay_listen (relay, &err)) {
                g_critical ("Failed to start relay: %s", err->message);
                return 1;
        }

        g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

        g_unix_signal_add (SIGINT, on_signal_quit, loop);
        g_unix_signal_add (SIGTERM, on_signal_quit, loop);

        g_main_loop_run (loop);

        g_message ("Quitting nostrum ...");

        return 0;
}