/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumMsgNotice object (Nostr NOTICE message)                             */
/* Implementation of nostrum-msg-notice.h                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-notice.h"
#include <json-glib/json-glib.h>

G_DEFINE_QUARK (nostrum-msg-notice-error-quark, nostrum_msg_notice_error)

struct _NostrumMsgNotice
{
        gchar *message; // owned
};

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumMsgNotice *
nostrum_msg_notice_new (void)
{
        NostrumMsgNotice *notice = g_new0 (NostrumMsgNotice, 1);
        return notice;
}

void
nostrum_msg_notice_free (NostrumMsgNotice *notice)
{
        if (!notice)
                return;

        g_free (notice->message);
        g_free (notice);
}

// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumMsgNotice *
nostrum_msg_notice_from_json (const char *json, GError **err)
{
        g_return_val_if_fail (json != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        NostrumMsgNotice *notice = nostrum_msg_notice_new ();

        // Parsing JSON string
        g_autoptr (JsonParser) parser = json_parser_new ();
        g_autoptr (GError) json_err = NULL;

        if (!json_parser_load_from_data (parser, json, -1, &json_err)) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "Error parsing NOTICE JSON: (%s, code=%d): %s",
                             g_quark_to_string (json_err->domain),
                             json_err->code,
                             json_err->message);
                goto error;
        }

        JsonNode *root = json_parser_get_root (parser);
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "NOTICE message must be a JSON array");
                goto error;
        }

        JsonArray *arr = json_node_get_array (root);
        gsize len = json_array_get_length (arr);

        if (len < 2) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "NOTICE message must have at least 2 elements");
                goto error;
        }

        // Element 0: "NOTICE"
        JsonNode *type_node = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (type_node) ||
            json_node_get_value_type (type_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "First element of NOTICE message must "
                             "be a string");
                goto error;
        }

        const char *type_str = json_node_get_string (type_node);
        if (g_strcmp0 (type_str, "NOTICE") != 0) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "First element of NOTICE message "
                             "must be \"NOTICE\"");
                goto error;
        }

        // Element 1: message (string)
        JsonNode *msg_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE (msg_node) ||
            json_node_get_value_type (msg_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_NOTICE_ERROR,
                             NOSTRUM_MSG_NOTICE_ERROR_PARSE,
                             "Second element of NOTICE message must be a string"
                             " (message)");
                goto error;
        }

        const char *msg_str = json_node_get_string (msg_node);
        nostrum_msg_notice_set_message (notice, msg_str);

        return notice; // transfer-full

error:
        nostrum_msg_notice_free (notice);
        return NULL;
}

gchar *
nostrum_msg_notice_to_json (const NostrumMsgNotice *notice)
{
        g_return_val_if_fail (notice != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_array (b);

        // Element 0: "NOTICE"
        json_builder_add_string_value (b, "NOTICE");

        // Element 1: message
        json_builder_add_string_value (b,
                                       notice->message
                                               ? notice->message
                                               : "");

        json_builder_end_array (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}

// =============================================================================
// GETTERS
// =============================================================================

const gchar *
nostrum_msg_notice_get_message (const NostrumMsgNotice *notice)
{
        g_return_val_if_fail (notice != NULL, NULL);
        return notice->message;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_msg_notice_set_message (NostrumMsgNotice *notice, const gchar *message)
{
        g_return_if_fail (notice != NULL);

        g_free (notice->message);
        notice->message = message ? g_strdup (message)
                                  : NULL;
}

