/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgNotice object.                                     */
/* A representation of a Nostr NOTICE message                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

// NIP-01 message:
// ["NOTICE", "<message>"]

#ifndef NOSTRUM_MSG_NOTICE_H
#define NOSTRUM_MSG_NOTICE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_NOTICE_ERROR_PARSE,
} NostrumNoticeError;

GQuark nostrum_msg_notice_error_quark (void);
#define NOSTRUM_MSG_NOTICE_ERROR (nostrum_msg_notice_error_quark ())

typedef struct _NostrumMsgNotice NostrumMsgNotice;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumMsgNotice *nostrum_msg_notice_new  (void);
void              nostrum_msg_notice_free (NostrumMsgNotice *notice);

// JSON CONVERSIONS ------------------------------------------------------------

NostrumMsgNotice *nostrum_msg_notice_from_json (const char  *json,
                                                GError     **error);

gchar         *nostrum_msg_notice_to_json   (const NostrumMsgNotice *notice);

// GETTERS ---------------------------------------------------------------------

const gchar   *nostrum_msg_notice_get_message (const NostrumMsgNotice *notice);


// SETTERS ---------------------------------------------------------------------

void           nostrum_msg_notice_set_message (NostrumMsgNotice       *notice,
                                               const gchar         *message);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgNotice, nostrum_msg_notice_free)

G_END_DECLS

#endif // NOSTRUM_MSG_NOTICE_H
