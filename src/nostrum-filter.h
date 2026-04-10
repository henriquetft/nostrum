/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumFilter object (a nostr subscription filter).          */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_FILTER_H
#define NOSTRUM_FILTER_H

#include <glib.h>
#include "nostrum-event.h"

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_FILTER_ERROR_PARSE,
        NOSTRUM_FILTER_UNKNOWN_ELEMENT,
} NostrumFilterError;

GQuark nostrum_filter_error_quark (void);
#define NOSTRUM_FILTER_ERROR (nostrum_filter_error_quark ())

typedef struct _NostrumFilter NostrumFilter;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumFilter     *nostrum_filter_new          (void);
void               nostrum_filter_free         (NostrumFilter *f);

NostrumFilter     *nostrum_filter_copy         (const NostrumFilter *f);

// OPERATIONS ------------------------------------------------------------------
gboolean           nostrum_filter_matches_event (const NostrumFilter *filter,
                                                 const NostrumEvent  *event);

gboolean           nostrum_filter_is_empty      (const NostrumFilter *f);
// JSON CONVERSIONS ------------------------------------------------------------
NostrumFilter  *nostrum_filter_from_json (const char *json_str, GError **err);
gchar          *nostrum_filter_to_json   (const NostrumFilter *filter);

// GETTERS ---------------------------------------------------------------------
const GPtrArray   *nostrum_filter_get_ids        (const NostrumFilter *f);
const GPtrArray   *nostrum_filter_get_authors    (const NostrumFilter *f);
const GArray      *nostrum_filter_get_kinds      (const NostrumFilter *f);
gint64             nostrum_filter_get_since      (const NostrumFilter *f);
gint64             nostrum_filter_get_until      (const NostrumFilter *f);
const GHashTable  *nostrum_filter_get_tags       (const NostrumFilter *f);
gint               nostrum_filter_get_limit      (const NostrumFilter *f);
const GPtrArray   *nostrum_filter_get_dedup_keys (const NostrumFilter *f);

// SETTERS ---------------------------------------------------------------------
void               nostrum_filter_take_ids     (NostrumFilter       *f,
                                                GPtrArray           *ids);
void               nostrum_filter_take_authors (NostrumFilter       *f,
                                                GPtrArray           *authors);
void               nostrum_filter_take_kinds   (NostrumFilter       *f,
                                                GArray              *kinds);
void               nostrum_filter_set_since    (NostrumFilter       *f,
                                                gint64               since);
void               nostrum_filter_set_until    (NostrumFilter       *f,
                                                gint64               until);


void               nostrum_filter_take_dedup_keys     (NostrumFilter       *f,
                                                       GPtrArray           *ids);
/**
 * nostrum_filter_take_tags:
 * @filter: a #NostrumFilter
 * @tags: (transfer full) (nullable): a #GHashTable mapping strings to
 * #GPtrArray of strings.
 *
 * Sets the tag table of @filter, taking ownership of @tags.
 * The previous table (if any) is freed.
 */
void               nostrum_filter_take_tags    (NostrumFilter       *f,
                                                GHashTable          *tags);

void               nostrum_filter_set_limit    (NostrumFilter       *f,
                                                gint                 limit);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC(NostrumFilter, nostrum_filter_free)

G_END_DECLS

#endif // NOSTRUM_FILTER_H