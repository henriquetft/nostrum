/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgClosed object.                                     */
/* A representation of Nostr CLOSED message                                   */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

// NIP-01 message:
// ["CLOSED", "<subscription_id>", "<message>"]

#ifndef NOSTRUM_MSG_CLOSED_H
#define NOSTRUM_MSG_CLOSED_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_CLOSED_ERROR_PARSE,
} NostrumClosedError;

GQuark nostrum_msg_closed_error_quark (void);
#define NOSTRUM_MSG_CLOSED_ERROR (nostrum_msg_closed_error_quark ())

typedef struct _NostrumMsgClosed NostrumMsgClosed;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------

NostrumMsgClosed *nostrum_msg_closed_new  (void);
void              nostrum_msg_closed_free (NostrumMsgClosed *closed);

// JSON CONVERSIONS ------------------------------------------------------------

NostrumMsgClosed *nostrum_msg_closed_from_json (const char  *json,
                                                GError     **error);

gchar            *nostrum_msg_closed_to_json   (const NostrumMsgClosed *closed);

// GETTERS ---------------------------------------------------------------------

const gchar *
nostrum_msg_closed_get_subscription_id (const NostrumMsgClosed *closed);

const gchar *
nostrum_msg_closed_get_message         (const NostrumMsgClosed *closed);

// SETTERS ---------------------------------------------------------------------

void nostrum_msg_closed_set_subscription_id (NostrumMsgClosed *closed,
                                             const gchar      *subscription_id);

void nostrum_msg_closed_set_message         (NostrumMsgClosed *closed,
                                             const gchar      *message);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgClosed, nostrum_msg_closed_free)

G_END_DECLS

#endif // NOSTRUM_MSG_CLOSED_H
