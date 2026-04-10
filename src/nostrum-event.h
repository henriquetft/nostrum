/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumEvent object (a representation of a Nostr event)      */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_EVENT_H
#define NOSTRUM_EVENT_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_EVENT_ERROR_INVALID_HEX,
        NOSTRUM_EVENT_ERROR_BAD_PUBKEY,
        NOSTRUM_EVENT_ERROR_BAD_SIG,
        NOSTRUM_EVENT_ERROR_SERIALIZE,
        NOSTRUM_EVENT_ERROR_VERIFY,
        NOSTRUM_EVENT_ERROR_PARSE,
} NostrumEventError;

GQuark nostrum_event_error_quark (void);
#define NOSTRUM_EVENT_ERROR (nostrum_event_error_quark ())

typedef struct _NostrumEvent NostrumEvent;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumEvent     *nostrum_event_new               (void);

void             nostrum_event_free               (NostrumEvent         *e);

// OPERATIONS ------------------------------------------------------------------
void             nostrum_event_compute_id        (NostrumEvent         *e,
                                                  GError              **err);

char             *nostrum_event_serialize        (const NostrumEvent   *e,
                                                  GError              **err);

gboolean         nostrum_event_verify_sig        (const NostrumEvent   *e,
                                                  GError               **err);

void             nostrum_event_add_tag           (NostrumEvent         *e,
                                                  const char           *tag,
                                                  ...);

gboolean         nostrum_event_is_addressable    (const NostrumEvent   *e);
gboolean         nostrum_event_is_ephemeral      (const NostrumEvent   *e);
gboolean         nostrum_event_is_replaceable    (const NostrumEvent   *e);
gboolean         nostrum_event_is_regular        (const NostrumEvent   *e);


// JSON CONVERSIONS ------------------------------------------------------------
NostrumEvent     *nostrum_event_from_json        (const char           *json,
                                                  GError              **err);
gchar            *nostrum_event_to_json          (const NostrumEvent   *event);

void             nostrum_event_print             (const NostrumEvent   *event);


// GETTERS ---------------------------------------------------------------------
const char      *nostrum_event_get_id            (const NostrumEvent   *event);
gint64           nostrum_event_get_storage_id    (const NostrumEvent   *event);
const char      *nostrum_event_get_pubkey        (const NostrumEvent   *event);
long             nostrum_event_get_created_at    (const NostrumEvent   *event);
int              nostrum_event_get_kind          (const NostrumEvent   *event);
const char      *nostrum_event_get_content       (const NostrumEvent   *event);
const char      *nostrum_event_get_sig           (const NostrumEvent   *event);
const GPtrArray *nostrum_event_get_tags          (const NostrumEvent   *event);
const char      *nostrum_event_get_dedup_key     (const NostrumEvent   *event);

// SETTERS ---------------------------------------------------------------------
void             nostrum_event_set_id            (NostrumEvent    *event,
                                                  const char      *id);
void             nostrum_event_set_storage_id    (NostrumEvent    *event,
                                                  gint64           storage_id);

void             nostrum_event_set_pubkey        (NostrumEvent    *event,
                                                  const char      *pubkey);

void             nostrum_event_set_created_at    (NostrumEvent    *event,
                                                  long             created_at);

void             nostrum_event_set_kind          (NostrumEvent    *event,
                                                  int              kind);

void             nostrum_event_set_content       (NostrumEvent    *event,
                                                  const char      *content);
                                                 
void             nostrum_event_set_sig           (NostrumEvent    *event,
                                                  const char      *sig);

void             nostrum_event_set_dedup_key     (NostrumEvent    *event,
                                                  const char      *dedup_key);

 /**
 * nostrum_event_take_tags:
 * @event: a #NostrumEvent
 * @tags: (transfer full) (nullable): tags are expected to be #GPtrArray of
 * #GPtrArray of strings. 
 *
 * Sets the tags array of @event, taking ownership of @tags.
 * The previous array (if any) is freed.
 */
void             nostrum_event_take_tags         (NostrumEvent    *event,
                                                  GPtrArray       *tags);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC(NostrumEvent, nostrum_event_free)

G_END_DECLS

#endif // NOSTRUM_EVENT_H

