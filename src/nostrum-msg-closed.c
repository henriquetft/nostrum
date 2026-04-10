/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumMsgClosed object (Nostr CLOSED message)                             */
/* Implementation of nostrum-msg-closed.h                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-closed.h"
#include <json-glib/json-glib.h>

G_DEFINE_QUARK (nostrum-msg-closed-error-quark, nostrum_msg_closed_error)

struct _NostrumMsgClosed
{
        gchar *subscription_id; // owned
        gchar *message;         // owned
};

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumMsgClosed *
nostrum_msg_closed_new (void)
{
        NostrumMsgClosed *closed = g_new0 (NostrumMsgClosed, 1);
        return closed;
}

void
nostrum_msg_closed_free (NostrumMsgClosed *closed)
{
        if (!closed)
                return;

        g_free (closed->subscription_id);
        g_free (closed->message);
        g_free (closed);
}

// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumMsgClosed *
nostrum_msg_closed_from_json (const char *json, GError **err)
{
        g_return_val_if_fail (json != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        NostrumMsgClosed *closed = nostrum_msg_closed_new ();

        // Parsing JSON string
        g_autoptr (JsonParser) parser = json_parser_new ();
        g_autoptr (GError) json_err = NULL;

        if (!json_parser_load_from_data (parser, json, -1, &json_err)) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "Error parsing CLOSED JSON: (%s, code=%d): %s",
                             g_quark_to_string (json_err->domain),
                             json_err->code,
                             json_err->message);
                goto error;
        }

        JsonNode *root = json_parser_get_root (parser);
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "CLOSED message must be a JSON array");
                goto error;
        }

        JsonArray *arr = json_node_get_array (root);
        gsize len = json_array_get_length (arr);

        if (len < 3) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "CLOSED message must have at least 3 elements");
                goto error;
        }

        // Element 0: "CLOSED"
        JsonNode *type_node = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (type_node) ||
            json_node_get_value_type (type_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "First element of CLOSED message "
                             "must be a string");
                goto error;
        }

        const char *type_str = json_node_get_string (type_node);
        if (g_strcmp0 (type_str, "CLOSED") != 0) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "First element of CLOSED message "
                             "must be \"CLOSED\"");
                goto error;
        }

        // Element 1: subscription id (string)
        JsonNode *id_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE (id_node) ||
            json_node_get_value_type (id_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "Second element of CLOSED message must be a string"
                             " (subscription id)");
                goto error;
        }

        const char *subs_id_str = json_node_get_string (id_node);

        // Element 2: message (string)
        JsonNode *msg_node = json_array_get_element (arr, 2);
        if (!JSON_NODE_HOLDS_VALUE (msg_node) ||
            json_node_get_value_type (msg_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSED_ERROR,
                             NOSTRUM_MSG_CLOSED_ERROR_PARSE,
                             "Third element of CLOSED message must be a string "
                             "(message)");
                goto error;
        }

        const char *msg_str = json_node_get_string (msg_node);

        nostrum_msg_closed_set_subscription_id (closed, subs_id_str);
        nostrum_msg_closed_set_message (closed, msg_str);

        return closed; // transfer-full

error:
        nostrum_msg_closed_free (closed);
        return NULL;
}

gchar *
nostrum_msg_closed_to_json (const NostrumMsgClosed *closed)
{
        g_return_val_if_fail (closed != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_array (b);

        // Element 0: "CLOSED"
        json_builder_add_string_value (b, "CLOSED");

        // Element 1: subscription id
        json_builder_add_string_value (b,
                                       closed->subscription_id
                                               ? closed->subscription_id
                                               : "");

        // Element 2: message
        json_builder_add_string_value (b,
                                       closed->message
                                               ? closed->message
                                               : "");

        json_builder_end_array (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}

// =============================================================================
// GETTERS
// =============================================================================

const gchar *
nostrum_msg_closed_get_subscription_id (const NostrumMsgClosed *closed)
{
        g_return_val_if_fail (closed != NULL, NULL);
        return closed->subscription_id;
}

const gchar *
nostrum_msg_closed_get_message (const NostrumMsgClosed *closed)
{
        g_return_val_if_fail (closed != NULL, NULL);
        return closed->message;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_msg_closed_set_subscription_id (NostrumMsgClosed  *closed,
                                        const gchar       *subscription_id)
{
        g_return_if_fail (closed != NULL);

        g_free (closed->subscription_id);
        closed->subscription_id = subscription_id ? g_strdup (subscription_id)
                                                  : NULL;
}

void
nostrum_msg_closed_set_message (NostrumMsgClosed *closed, const gchar *message)
{
        g_return_if_fail (closed != NULL);

        g_free (closed->message);
        closed->message = message ? g_strdup (message)
                                  : NULL;
}


