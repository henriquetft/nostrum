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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumRelay, nostrum_relay_free)

G_END_DECLS

#endif // NOSTRUM_RELAY_H