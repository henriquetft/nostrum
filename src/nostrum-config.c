/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumRelayConfig object (implementation of nostrum-config.h)             */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-config.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define G_LOG_DOMAIN "nostrum-config"

G_DEFINE_QUARK (nostrum-config-error-quark, nostrum_config_error)

// =============================================================================
// HELPERS -- DECLARATIONS
// =============================================================================

static gboolean
get_required_string (GKeyFile *kf,
                     const char *group,
                     const char *key,
                     char **out,
                     GError **error);

static gboolean
get_optional_string (GKeyFile *kf,
                     const char *group,
                     const char *key,
                     char **out);

static gboolean
get_required_int (GKeyFile *kf,
                  const char *group,
                  const char *key,
                  int *out,
                  int min,
                  int max,
                  GError **error);

static gboolean
get_optional_int (GKeyFile *kf,
                  const char *group,
                  const char *key,
                  int *out,
                  int min,
                  int max,
                  GError **error);

static gboolean
is_file_readable (const gchar *path, GError **error);



// =============================================================================
// IMPLEMENTATION
// =============================================================================

void
nostrum_relay_config_init (struct NostrumRelayConfig *cfg)
{
        g_return_if_fail (cfg != NULL);

        *cfg = (struct NostrumRelayConfig) {
                .server_host = NULL,
                .server_http_port = 0,
                .server_https_port = 0,
                .server_tls_cert = NULL,
                .server_tls_key = NULL,

                .db_path = NULL,
                .db_type = NULL,

                .info_name = NULL,
                .info_description = NULL,
                .info_contact = NULL,
        };
}

void
nostrum_relay_config_clear (struct NostrumRelayConfig *cfg)
{
        g_return_if_fail (cfg != NULL);

        g_clear_pointer (&cfg->server_host, g_free);
        g_clear_pointer (&cfg->server_tls_cert, g_free);
        g_clear_pointer (&cfg->server_tls_key, g_free);

        g_clear_pointer (&cfg->db_path, g_free);
        g_clear_pointer (&cfg->db_type, g_free);

        g_clear_pointer (&cfg->info_name, g_free);
        g_clear_pointer (&cfg->info_description, g_free);
        g_clear_pointer (&cfg->info_contact, g_free);

        cfg->server_http_port = 0;
        cfg->server_https_port = 0;
}

void
nostrum_relay_config_copy (struct NostrumRelayConfig *dst,
                           const struct NostrumRelayConfig *src)
{
        g_return_if_fail (dst != NULL);
        g_return_if_fail (src != NULL);

        nostrum_relay_config_clear (dst);

        dst->server_http_port = src->server_http_port;
        dst->server_https_port = src->server_https_port;
        dst->server_host = g_strdup (src->server_host);
        dst->server_tls_cert = g_strdup (src->server_tls_cert);
        dst->server_tls_key = g_strdup (src->server_tls_key);

        dst->db_path = g_strdup (src->db_path);
        dst->db_type = g_strdup (src->db_type);

        dst->info_name = g_strdup (src->info_name);
        dst->info_description = g_strdup (src->info_description);
        dst->info_contact = g_strdup (src->info_contact);
}


gboolean
nostrum_relay_config_load (struct NostrumRelayConfig *cfg, GError **error)
{
        const gchar *env_path;
        const gchar *paths[3];
        guint i;

        if (!cfg) {
                g_set_error (error,
                             G_FILE_ERROR,
                             G_FILE_ERROR_INVAL,
                             "Config struct is NULL");
                return FALSE;
        }

        env_path = g_getenv ("NOSTRUM_CONFIG");

        if (!env_path) {
                g_debug ("NOSTRUM_CONFIG environment variable is not set");
        }

        paths[0] = env_path;
        paths[1] = "/etc/nostrum.ini";
        paths[2] = "./nostrum.ini";

        for (i = 0; i < 3; i++) {
                const gchar *path = paths[i];

                if (!path || *path == '\0')
                        continue;

                GError *local_error = NULL;

                if (!is_file_readable (path, &local_error)) {
                        if (local_error) {
                                g_debug ("Skipping config '%s': %s",
                                         path, local_error->message);
                                g_clear_error (&local_error);
                        }
                        continue;
                }

                if (nostrum_relay_config_load_from_file (path,
                                                         cfg,
                                                         &local_error)) {
                        g_message ("Loaded config from: %s", path);

                        nostrum_relay_config_validate (cfg, &local_error);
                        if (local_error) {
                                g_warning ("Config loaded from '%s' is invalid: %s",
                                           path, local_error->message);
                                g_clear_error (&local_error);
                                continue;
                        }
                        return TRUE;
                }

                g_warning ("Failed to load config from '%s': %s",
                           path, local_error ? local_error->message
                                             : "unknown error");

                g_clear_error (&local_error);
        }

        g_set_error (error,
                     G_FILE_ERROR,
                     G_FILE_ERROR_NOENT,
                     "No valid readable configuration file found (checked "
                     "NOSTRUM_CONFIG, /etc/nostrum.ini, ./nostrum.ini)");

        return FALSE;
}

gboolean
nostrum_relay_config_load_from_file (const char *path,
                                     struct NostrumRelayConfig *cfg,
                                     GError **error)
{
        GKeyFile *kf = g_key_file_new ();
        GError *err = NULL;

        if (!g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, &err)) {
                g_propagate_error (error, err);
                g_key_file_free (kf);
                return FALSE;
        }

        // SERVER SECTION ------------------------------------------------------
        if (!get_required_string (kf, "server", "host", &cfg->server_host,
                                  error))
                goto error;

        if (!get_optional_int (kf, "server", "http_port",
                               &cfg->server_http_port, 0, 65535, error))
                goto error;

        if (!get_optional_int (kf, "server", "https_port",
                               &cfg->server_https_port, 0, 65535, error))
                goto error;

        get_optional_string (kf, "server", "tls_cert", &cfg->server_tls_cert);
        get_optional_string (kf, "server", "tls_key", &cfg->server_tls_key);

        // DATABASE SECTION ----------------------------------------------------
        if (!get_required_string (kf, "database", "type", &cfg->db_type, error))
                goto error;

        if (!get_required_string (kf, "database", "path", &cfg->db_path, error))
                goto error;

        // INFO SECTION --------------------------------------------------------
        get_optional_string (kf, "info", "name", &cfg->info_name);
        get_optional_string (kf, "info", "description", &cfg->info_description);
        get_optional_string (kf, "info", "contact", &cfg->info_contact);

        // SET DEFAULT VALUES IF NEEDED ----------------------------------------
        if (!cfg->info_name)
                cfg->info_name = g_strdup ("Nostrum Relay");
        if (!cfg->info_description)
                cfg->info_description = g_strdup ("Relay Nostrum");
        if (!cfg->info_contact)
                cfg->info_contact = g_strdup ("");
        // ---------------------------------------------------------------------
        g_key_file_free (kf);
        return TRUE;

error:
        g_key_file_free (kf);
        return FALSE;
}



gboolean
nostrum_relay_config_validate (struct NostrumRelayConfig *cfg, GError **error)
{
        g_return_val_if_fail (cfg != NULL, FALSE);

        // SERVER HOST VALIDATIONS ---------------------------------------------
        if (cfg->server_host == NULL) {
                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_MISSING_VALUE,
                             "Server Host is required");
                return FALSE;
        }


        if (g_strcmp0(cfg->server_host, "0.0.0.0") != 0 &&
            g_strcmp0(cfg->server_host, "::") != 0 &&
            g_strcmp0(cfg->server_host, "127.0.0.1") != 0 &&
            g_strcmp0(cfg->server_host, "::1") != 0) {
                GInetAddress *addr =
                    g_inet_address_new_from_string (cfg->server_host);
                if (!addr) {
                        g_set_error (error,
                                     NOSTRUM_CONFIG_ERROR,
                                     NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                                     "Invalid server_host: %s",
                                     cfg->server_host);
                        return FALSE;
                }

                g_object_unref (addr);
        }

        // PORT VALIDATIONS ----------------------------------------------------
        if (cfg->server_http_port == 0 && cfg->server_https_port == 0) {
                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_MISSING_VALUE,
                             "At least one of server_http_port or "
                             "server_https_port must be set");
                return FALSE;
        }

        if (cfg->server_http_port != 0 && cfg->server_https_port != 0 &&
                cfg->server_http_port == cfg->server_https_port) {
                        g_set_error(error, NOSTRUM_CONFIG_ERROR,
                                    NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                                    "HTTP and HTTPS ports must be different");
                return FALSE;
        }

        // TLS VALIDATION ------------------------------------------------------
        if ((cfg->server_tls_cert && !cfg->server_tls_key) ||
            (!cfg->server_tls_cert && cfg->server_tls_key)) {

                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                             "Both tls_cert and tls_key must be provided "
                             "together");
                return FALSE;
        }

        if (cfg->server_tls_cert != NULL) {
                GError *tmp_error = NULL;

                if (!is_file_readable (cfg->server_tls_cert, &tmp_error)) {
                        g_set_error (error,
                                     NOSTRUM_CONFIG_ERROR,
                                     NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                                     "%s", tmp_error ? tmp_error->message : "");
                        g_clear_error (&tmp_error);
                        return FALSE;
                }
        }

        if (cfg->server_tls_key != NULL) {
                GError *tmp_error = NULL;

                if (!is_file_readable (cfg->server_tls_key, &tmp_error)) {
                        g_set_error (error,
                                     NOSTRUM_CONFIG_ERROR,
                                     NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                                     "%s", tmp_error ? tmp_error->message : "");
                        g_clear_error (&tmp_error);
                        return FALSE;
                }
        }

        // DATABASE VALIDATIONS ------------------------------------------------

        if (cfg->db_type == NULL) {
                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_MISSING_VALUE,
                             "Database Type is required");
                return FALSE;
        }

        if (g_strcmp0(cfg->db_type, "sqlite") != 0) {
                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                             "Invalid Database Type");
                return FALSE;
        }

        if (cfg->db_path == NULL) {
                g_set_error (error, NOSTRUM_CONFIG_ERROR,
                             NOSTRUM_CONFIG_ERROR_MISSING_VALUE,
                             "Database Path is required for sqlite");
                return FALSE;
        }


        GError *tmp_error = NULL;
        if (!is_file_readable (cfg->db_path, &tmp_error)) {
                g_set_error(error,
                            NOSTRUM_CONFIG_ERROR,
                            NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
                            "%s", tmp_error ? tmp_error->message : "");
                g_clear_error (&tmp_error);
                return FALSE;
        }
        
        return TRUE;
}

gchar *
nostrum_relay_config_to_string (const struct NostrumRelayConfig *cfg)
{
        if (!cfg)
                return g_strdup ("(null config)");

        return g_strdup_printf (
            "=== Server ===\n"
            "host: %s\n"
            "http_port: %u\n"
            "https_port: %u\n"
            "tls_cert: %s\n"
            "tls_key: %s\n"

            "\n=== Database ===\n"
            "type: %s\n"
            "path: %s\n"

            "\n=== Info ===\n"
            "name: %s\n"
            "description: %s\n"
            "contact: %s\n",

            cfg->server_host ? cfg->server_host : "(null)",
            cfg->server_http_port,
            cfg->server_https_port,
            cfg->server_tls_cert ? cfg->server_tls_cert : "(null)",
            cfg->server_tls_key ? cfg->server_tls_key : "(null)",

            cfg->db_type ? cfg->db_type : "(null)",
            cfg->db_path ? cfg->db_path : "(null)",

            cfg->info_name ? cfg->info_name : "(null)",
            cfg->info_description ? cfg->info_description : "(null)",
            cfg->info_contact ? cfg->info_contact : "(null)");
}


// =============================================================================
// HELPERS -- IMPLEMENTATION
// =============================================================================

static gboolean
get_required_string (GKeyFile *kf,
                     const char *group,
                     const char *key,
                     char **out,
                     GError **error)
{
        GError *err = NULL;
        char *val = g_key_file_get_string (kf, group, key, &err);

        if (err || !val || *val == '\0') {
                g_clear_error (&err);
                g_set_error (error, G_KEY_FILE_ERROR,
                             G_KEY_FILE_ERROR_INVALID_VALUE,
                             "[%s] %s is required and invalid", group, key);
                return FALSE;
        }

        *out = val;
        return TRUE;
}

static gboolean
get_optional_string (GKeyFile *kf,
                     const char *group,
                     const char *key,
                     char **out)
{
        *out = g_key_file_get_string (kf, group, key, NULL);
        return TRUE;
}

static gboolean
get_required_int (GKeyFile *kf,
                  const char *group,
                  const char *key,
                  int *out,
                  int min,
                  int max,
                  GError **error)
{
        GError *err = NULL;

        int val = g_key_file_get_integer (kf, group, key, &err);

        if (err) {
                g_clear_error (&err);
                g_set_error (error, G_KEY_FILE_ERROR,
                             G_KEY_FILE_ERROR_INVALID_VALUE,
                             "[%s] %s must be a valid integer", group, key);
                return FALSE;
        }

        if (val < min || val > max) {
                g_set_error (error, G_KEY_FILE_ERROR,
                             G_KEY_FILE_ERROR_INVALID_VALUE,
                             "[%s] %s out of range (%d-%d)",
                             group, key, min, max);
                return FALSE;
        }

        *out = val;
        return TRUE;
}

static gboolean
get_optional_int (GKeyFile *kf,
                  const char *group,
                  const char *key,
                  int *out,
                  int min,
                  int max,
                  GError **error)
{
        GError *err = NULL;

        if (!g_key_file_has_key (kf, group, key, NULL)) {
                *out = 0;
                return TRUE;
        }

        int val = g_key_file_get_integer (kf, group, key, &err);

        if (err) {
                g_clear_error (&err);
                g_set_error (error, G_KEY_FILE_ERROR,
                             G_KEY_FILE_ERROR_INVALID_VALUE,
                             "[%s] %s must be a valid integer", group, key);
                return FALSE;
        }

        if (val < min || val > max) {
                g_set_error (error, G_KEY_FILE_ERROR,
                             G_KEY_FILE_ERROR_INVALID_VALUE,
                             "[%s] %s out of range (%d-%d)", group, key,
                             min, max);
                return FALSE;
        }

        *out = val;
        return TRUE;
}


static gboolean
is_file_readable (const gchar *path, GError **error)
{
        struct stat st;

        if (!path || *path == '\0') {
                g_set_error (error,
                             G_FILE_ERROR,
                             G_FILE_ERROR_INVAL,
                             "No file path specified");
                return FALSE;
        }

        if (g_stat (path, &st) != 0) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             "Cannot stat file: %s", path);
                return FALSE;
        }

        if (!S_ISREG (st.st_mode)) {
                g_set_error (error,
                             G_FILE_ERROR,
                             G_FILE_ERROR_INVAL,
                             "Not a regular file: %s", path);
                return FALSE;
        }

        if (g_access (path, R_OK) != 0) {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             "No read permission for file: %s", path);
                return FALSE;
        }

        return TRUE;
}
