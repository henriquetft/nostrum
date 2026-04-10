/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of the NostrumSubscription object                               */
/* Represents a Nostr subscription, containing ID and filters.                */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_SUBSCRIPTION_H
#define NOSTRUM_SUBSCRIPTION_H

#include <glib.h>
#include "nostrum-event.h"

G_BEGIN_DECLS

typedef struct _NostrumSubscription NostrumSubscription;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumSubscription *
nostrum_subscription_new   (const gchar         *id);

void
nostrum_subscription_free  (NostrumSubscription *sub);

// OPERATIONS ------------------------------------------------------------------
gboolean
nostrum_subscription_matches_event (const NostrumSubscription  *sub,
                                    const NostrumEvent         *event);

// GETTERS ---------------------------------------------------------------------
const gchar *
nostrum_subscription_get_id       (const NostrumSubscription *sub);

const GPtrArray *
nostrum_subscription_get_filters  (const NostrumSubscription *sub);

// SETTERS ---------------------------------------------------------------------
void       nostrum_subscription_set_id        (NostrumSubscription    *sub,
                                               const gchar            *id);

void       nostrum_subscription_take_filters  (NostrumSubscription    *sub,
                                               GPtrArray              *filters);

// -----------------------------------------------------------------------------

G_END_DECLS

#endif // NOSTRUM_SUBSCRIPTION_H