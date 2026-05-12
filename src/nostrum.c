/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Main file of Relay Nostrum                                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-config.h"
#include "nostrum-relay.h"
#include "nostrum-version.h"
#include <glib-unix.h>
#include <glib.h>
#include <locale.h>

#define G_LOG_DOMAIN "nostrum"

static int
startup (gchar *arg_config_path);

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

        // Setup locale to C POSIX
        setlocale(LC_ALL, "C");
        setenv("LC_ALL", "C", 1);

        gint    retcode  = EXIT_FAILURE;
        GError *error    = NULL;

        // Args from cmdlinet parsing
        gboolean  show_version = FALSE;
        gchar    *config_path  = NULL;

        GOptionEntry cmd_entries[] = {
                { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
                  "Print version information and quit", NULL },

                { "config", 'c', 0, G_OPTION_ARG_STRING, &config_path,
                  "Path to configuration file", "FILE" },

                { 0 }
        };

        GOptionContext *context = g_option_context_new ("");
        g_option_context_add_main_entries (context, cmd_entries, NULL);

        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_printerr ("Option parsing failed: %s\n", error->message);
                g_error_free (error);
                retcode = EXIT_FAILURE;
                goto end;
        }

        if (show_version) {
                g_print ("%s\n", NOSTRUM_VERSION);
                retcode = EXIT_SUCCESS;
                goto end;
        }

        retcode = startup (config_path);

end:
        g_option_context_free (context);
        return retcode;
}

int
startup(gchar *arg_config_path)
{
        g_message ("    _   __           __");
        g_message ("   / | / /___  _____/ /________  ______ ___");
        g_message ("  /  |/ / __ \\/ ___/ __/ ___/ / / / __ `__ \\");
        g_message (" / /|  / /_/ (__  ) /_/ /  / /_/ / / / / / /");
        g_message ("/_/ |_/\\____/____/\\__/_/   \\__,_/_/ /_/ /_/");
        g_message ("                          v%s", NOSTRUM_VERSION);
        g_message ("");
        g_message ("Starting nostrum v%s ...", NOSTRUM_VERSION);

        // LOAD CONFIGURATION --------------------------------------------------
        struct NostrumRelayConfig cfg;
        nostrum_relay_config_init (&cfg);

        if (arg_config_path) {
                g_message ("Loading config from: '%s'", arg_config_path);
                GError *error = NULL;
                if (!nostrum_relay_config_load_from_file (arg_config_path,
                                                         &cfg,
                                                         &error)) {
                        g_critical ("Failed to load config file: %s",
                                    error ? error->message : "unknown error");
                        g_clear_error (&error);
                        return EXIT_FAILURE;
                }
                g_message ("Loaded config from: %s", arg_config_path);
        } else {
                GError *error = NULL;
                gboolean result = nostrum_relay_config_load (&cfg, &error);
                if (!result) {
                        g_critical ("Failed to load config file: %s",
                                    error ? error->message : "unknown error");
                        g_clear_error (&error);
                        return EXIT_FAILURE;
                }
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
