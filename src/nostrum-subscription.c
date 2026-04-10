/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumSubscription object (implementation of nostrum-subscription.h)      */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-subscription.h"
#include "nostrum-filter.h"

#include <glib.h>

struct _NostrumSubscription
{
        gchar      *id;       // owned
        GPtrArray  *filters;  // owned, NostrumFilter*
};

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumSubscription *
nostrum_subscription_new (const gchar *id)
{
        NostrumSubscription *sub = g_new0 (NostrumSubscription, 1);
        sub->id = id ? g_strdup (id)
                     : NULL;
        return sub;
}


void
nostrum_subscription_free (NostrumSubscription *sub)
{
        if (!sub)
                return;
        g_clear_pointer (&sub->id, g_free);
        if (sub->filters) {
                g_ptr_array_unref (sub->filters);
                sub->filters = NULL;
        }
        g_free (sub);
}

// =============================================================================
// OPERARTIONS
// =============================================================================

gboolean
nostrum_subscription_matches_event (const NostrumSubscription *sub,
                                    const NostrumEvent        *event)
{
        g_return_val_if_fail (sub   != NULL,  FALSE);
        g_return_val_if_fail (event != NULL,  FALSE);

        const GPtrArray *filters = nostrum_subscription_get_filters (sub);

        if (!filters || filters->len == 0)
                return FALSE;

        for (guint i = 0; i < filters->len; i++) {
                const NostrumFilter *f =
                    g_ptr_array_index ((GPtrArray *)filters, i);
                if (nostrum_filter_matches_event (f, event))
                        return TRUE;
        }

        return FALSE;
}

// =============================================================================
// GETTERS
// =============================================================================

const gchar *
nostrum_subscription_get_id (const NostrumSubscription *sub)
{
        g_return_val_if_fail (sub != NULL, NULL);
        return sub->id;
}


const GPtrArray *
nostrum_subscription_get_filters (const NostrumSubscription *sub)
{
        g_return_val_if_fail (sub != NULL, NULL);
        return sub->filters;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_subscription_set_id (NostrumSubscription *sub, const gchar *id)
{
        g_return_if_fail (sub != NULL);
        g_free (sub->id);
        sub->id = id ? g_strdup (id)
                     : NULL;
}

void
nostrum_subscription_take_filters (NostrumSubscription *sub, GPtrArray *filters)
{
        g_return_if_fail (sub != NULL);
        if (sub->filters)
                g_ptr_array_unref (sub->filters);
        sub->filters = filters;
}
