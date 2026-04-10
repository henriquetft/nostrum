/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumMsgOk object (Nostr OK message)                                     */
/* Implementation of nostrum-msg-ok.h                                         */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-ok.h"

struct _NostrumMsgOk
{
        char      *event_id;    // owned
        gboolean   accepted;
        char      *message;     // owned
};

G_DEFINE_QUARK (nostrum-msg-ok-error-quark, nostrum_msg_ok_error)

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumMsgOk *
nostrum_msg_ok_new (void)
{
        NostrumMsgOk *ok = g_new0 (NostrumMsgOk, 1);
        return ok;
}

void
nostrum_msg_ok_free (NostrumMsgOk *ok)
{
        if (!ok)
                return;

        g_free (ok->event_id);
        g_free (ok->message);
        g_free (ok);
}

// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumMsgOk *
nostrum_msg_ok_from_json (const char *json, GError **err)
{
        g_return_val_if_fail (json != NULL, NULL);

        NostrumMsgOk *ok = nostrum_msg_ok_new ();

        // Parsing JSON string
        g_autoptr(JsonParser) p = json_parser_new ();
        g_autoptr(GError) json_err = NULL;
        if (!json_parser_load_from_data (p, json, -1, &json_err)) {
                g_set_error(err,
                            NOSTRUM_MSG_OK_ERROR,
                            NOSTRUM_MSG_OK_ERROR_PARSE,
                            "Error parsing ok JSON: (%s, code=%d): %s",
                            g_quark_to_string(json_err->domain),
                            json_err->code,
                            json_err->message);
                goto error;
        }

        JsonNode *root = json_parser_get_root (p);
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "OK message must be a JSON array");
                goto error;
        }

        JsonArray *arr = json_node_get_array (root);
        gsize len = json_array_get_length (arr);

        if (len < 4) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "OK message must have at least 4 elements");
                goto error;
        }

        // Element 0: "OK"
        JsonNode *type_node = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (type_node) ||
            json_node_get_value_type (type_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "First element of OK message must be a string");
                goto error;
        }

        const char *type_str = json_node_get_string (type_node);
        if (g_strcmp0 (type_str, "OK") != 0) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "First element of OK message must be \"OK\"");
                goto error;
        }

        // Element 1: event_id (string)
        JsonNode *id_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE (id_node) ||
            json_node_get_value_type (id_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "Second element of OK message must be a string "
                             "(event id)");
                goto error;
        }

        const char *id_str = json_node_get_string (id_node);

        // Element 2: accepted (boolean)
        JsonNode *accepted_node = json_array_get_element (arr, 2);
        if (!JSON_NODE_HOLDS_VALUE (accepted_node) ||
            json_node_get_value_type (accepted_node) != G_TYPE_BOOLEAN) {
                g_set_error (err,
                             NOSTRUM_MSG_OK_ERROR,
                             NOSTRUM_MSG_OK_ERROR_PARSE,
                             "Third element of OK message must be a boolean");
                goto error;
        }

        gboolean accepted = json_node_get_boolean (accepted_node);

        // Element 3: message (string)
        JsonNode *msg_node = json_array_get_element (arr, 3);
        const char *msg_str = NULL;

        if (msg_node != NULL) {
                if (!JSON_NODE_HOLDS_VALUE (msg_node) ||
                    json_node_get_value_type (msg_node) != G_TYPE_STRING) {
                        g_set_error (err,
                                     NOSTRUM_MSG_OK_ERROR,
                                     NOSTRUM_MSG_OK_ERROR_PARSE,
                                     "Fourth element of OK message must be "
                                     "a string");
                        return NULL;
                }
                msg_str = json_node_get_string (msg_node);
        }

        // Filling NostrumMsgOk
        nostrum_msg_ok_set_id (ok, id_str);
        nostrum_msg_ok_set_accepted (ok, accepted);
        nostrum_msg_ok_set_message (ok, msg_str);

        return ok; // transfer-full
error:
        nostrum_msg_ok_free (ok);
        return NULL;
}


gchar *
nostrum_msg_ok_to_json (const NostrumMsgOk *ok)
{
        g_return_val_if_fail (ok != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_array (b);

        json_builder_add_string_value (b, "OK");
        json_builder_add_string_value (b, ok->event_id ? ok->event_id : "");
        json_builder_add_boolean_value (b, ok->accepted);
        json_builder_add_string_value (b, ok->message ? ok->message : "");

        json_builder_end_array (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}

// =============================================================================
// GETTERS
// =============================================================================

const char *
nostrum_msg_ok_get_id (const NostrumMsgOk *ok)
{
        g_return_val_if_fail (ok != NULL, NULL);
        return ok->event_id;
}

gboolean
nostrum_msg_ok_get_accepted (const NostrumMsgOk *ok)
{
        g_return_val_if_fail (ok != NULL, FALSE);
        return ok->accepted;
}

const char *
nostrum_msg_ok_get_message (const NostrumMsgOk *ok)
{
        g_return_val_if_fail (ok != NULL, NULL);
        return ok->message;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_msg_ok_set_id (NostrumMsgOk *ok, const char *event_id)
{
        g_return_if_fail (ok != NULL);

        g_free (ok->event_id);
        ok->event_id = event_id ? g_strdup (event_id) : NULL;
}

void
nostrum_msg_ok_set_accepted (NostrumMsgOk *ok, gboolean accepted)
{
        g_return_if_fail (ok != NULL);
        ok->accepted = accepted;
}

void
nostrum_msg_ok_set_message (NostrumMsgOk *ok, const char *msg)
{
        g_return_if_fail (ok != NULL);

        g_free (ok->message);
        ok->message = msg ? g_strdup (msg)
                          : NULL;
}
