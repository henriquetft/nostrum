/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumRelay object (a nostr relay / WS server)              */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_RELAY_H
#define NOSTRUM_RELAY_H

#include <glib.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

struct NostrumRelayConfig
{
        // server section
        gchar    *server_host;
        guint16   server_http_port;
        guint16   server_https_port;
        gchar    *server_tls_cert;
        gchar    *server_tls_key;

        // database section
        gchar    *db_type;
        gchar    *db_path;

        // info section
        gchar    *info_name;
        gchar    *info_description;
        gchar    *info_contact;
};

typedef struct _NostrumRelay NostrumRelay;


/**
 * nostrum_relay_new:
 * @cfg: (not nullable) (transfer none): configuration parameters.
 *
 * Creates a new NostrumRelay using the provided configuration.
 * The configuration structure is not retained; its contents are
 * copied internally.
 */
NostrumRelay *nostrum_relay_new        (const struct NostrumRelayConfig  *cfg);

void          nostrum_relay_free       (NostrumRelay   *relay);


SoupServer   *nostrum_relay_get_server (NostrumRelay   *relay);

gboolean      nostrum_relay_listen     (NostrumRelay   *relay,
                                        GError        **error);

void     nostrum_relay_config_init     (struct NostrumRelayConfig        *cfg);
void     nostrum_relay_config_clear    (struct NostrumRelayConfig        *cfg);
void     nostrum_relay_config_copy     (struct NostrumRelayConfig        *dst,
                                        const struct NostrumRelayConfig  *src);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumRelay, nostrum_relay_free)

G_END_DECLS

#endif // NOSTRUM_RELAY_H