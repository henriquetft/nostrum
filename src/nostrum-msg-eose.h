/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgEose object.                                       */
/* A representation of Nostr EOSE message                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

// NIP-01 message:
// ["EOSE", "<subscription_id>"]

#ifndef NOSTRUM_MSG_EOSE_H
#define NOSTRUM_MSG_EOSE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_EOSE_ERROR_PARSE,
} NostrumEoseError;

typedef struct _NostrumMsgEose NostrumMsgEose;

GQuark nostrum_msg_eose_error_quark (void);
#define NOSTRUM_MSG_EOSE_ERROR (nostrum_msg_eose_error_quark ())

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------

NostrumMsgEose *nostrum_msg_eose_new  (void);
void            nostrum_msg_eose_free (NostrumMsgEose *eose);

// JSON CONVERSIONS ------------------------------------------------------------

NostrumMsgEose *nostrum_msg_eose_from_json (const char  *json,
                                            GError     **error);

gchar       *nostrum_msg_eose_to_json   (const NostrumMsgEose *eose);

// GETTERS ---------------------------------------------------------------------

const gchar *nostrum_msg_eose_get_subscription_id (const NostrumMsgEose *eose);

// SETTERS ---------------------------------------------------------------------

void         nostrum_msg_eose_set_subscription_id (NostrumMsgEose     *eose,
                                                  const gchar         *subs_id);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgEose, nostrum_msg_eose_free)

G_END_DECLS

#endif // NOSTRUM_MSG_EOSE_H
