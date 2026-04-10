/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgClose object.                                      */
/* A representation of Nostr CLOSE message                                    */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

// NIP-01 message:
// ["CLOSE", "<subscription_id>"]

#ifndef NOSTRUM_MSG_CLOSE_H
#define NOSTRUM_MSG_CLOSE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_CLOSE_ERROR_PARSE,
} NostrumCloseError;

GQuark nostrum_msg_close_error_quark (void);
#define NOSTRUM_MSG_CLOSE_ERROR (nostrum_msg_close_error_quark ())

typedef struct _NostrumMsgClose NostrumMsgClose;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------

NostrumMsgClose *nostrum_msg_close_new  (void);
void             nostrum_msg_close_free (NostrumMsgClose *close);

// JSON CONVERSIONS ------------------------------------------------------------

NostrumMsgClose *nostrum_msg_close_from_json (const char    *json,
                                              GError       **error);

gchar        *nostrum_msg_close_to_json   (const NostrumMsgClose *close);

// GETTERS ---------------------------------------------------------------------

const gchar *
nostrum_msg_close_get_subscription_id (const NostrumMsgClose *close);

// SETTERS ---------------------------------------------------------------------
void        nostrum_msg_close_set_subscription_id (NostrumMsgClose   *close,
                                                   const gchar        *subs_id);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgClose, nostrum_msg_close_free)

G_END_DECLS

#endif // NOSTRUM_MSG_CLOSE_H
