/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgOk object.                                         */
/* A representation of Nostr Ok message                                       */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

// NIP-01 message:
// ["OK", <id>, <true/false>, <message>]

#ifndef NOSTRUM_MSG_OK_H
#define NOSTRUM_MSG_OK_H

#include <glib.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_OK_ERROR_PARSE,
} NostrumOkError;

GQuark nostrum_msg_ok_error_quark (void);
#define NOSTRUM_MSG_OK_ERROR (nostrum_msg_ok_error_quark ())

typedef struct _NostrumMsgOk NostrumMsgOk;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumMsgOk *      nostrum_msg_ok_new             (void);
void                nostrum_msg_ok_free            (NostrumMsgOk *ok);

// JSON CONVERSIONS ------------------------------------------------------------
NostrumMsgOk *      nostrum_msg_ok_from_json       (const char     *json,
                                                    GError        **err);

gchar *          nostrum_msg_ok_to_json         (const NostrumMsgOk *ok);

// GETTERS ---------------------------------------------------------------------
const char *     nostrum_msg_ok_get_id          (const NostrumMsgOk *ok);
gboolean         nostrum_msg_ok_get_accepted    (const NostrumMsgOk *ok);
const char *     nostrum_msg_ok_get_message     (const NostrumMsgOk *ok);

// SETTERS ---------------------------------------------------------------------
void             nostrum_msg_ok_set_id          (NostrumMsgOk      *ok,
                                                 const char        *event_id);

void             nostrum_msg_ok_set_accepted    (NostrumMsgOk      *ok,
                                                 gboolean           accepted);
                                                 
void             nostrum_msg_ok_set_message     (NostrumMsgOk      *ok,
                                                 const char        *msg);
// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgOk, nostrum_msg_ok_free)

G_END_DECLS

#endif // NOSTRUM_MSG_OK_H
