/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumRelayConfig (load config file)                        */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_CONFIG_H
#define NOSTRUM_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_CONFIG_ERROR_INVALID_VALUE,
        NOSTRUM_CONFIG_ERROR_MISSING_VALUE,
} NostrumConfigError;

GQuark nostrum_config_error_quark (void);
#define NOSTRUM_CONFIG_ERROR (nostrum_config_error_quark ())

struct NostrumRelayConfig
{
        // Server section
        gchar    *server_host;
        guint16   server_http_port;
        guint16   server_https_port;
        gchar    *server_tls_cert;
        gchar    *server_tls_key;

        // Database section
        gchar    *db_type;
        gchar    *db_path;

        // Info section
        gchar    *info_name;
        gchar    *info_description;
        gchar    *info_contact;
};

void     nostrum_relay_config_init     (struct NostrumRelayConfig        *cfg);
void     nostrum_relay_config_clear    (struct NostrumRelayConfig        *cfg);
void     nostrum_relay_config_copy     (struct NostrumRelayConfig        *dst,
                                        const struct NostrumRelayConfig  *src);

gboolean
nostrum_relay_config_load (struct NostrumRelayConfig *cfg, GError **error);

gboolean
nostrum_relay_config_load_from_file (const char                  *path,
                                     struct NostrumRelayConfig   *cfg,
                                     GError                     **error);

gchar   *nostrum_relay_config_to_string (const struct NostrumRelayConfig  *cfg);

gboolean
nostrum_relay_config_validate (struct NostrumRelayConfig *cfg, GError **error);

G_END_DECLS

#endif // NOSTRUM_CONFIG_H