/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumMsgClose object (Nostr CLOSE message)                               */
/* Implementation of nostrum-msg-close.h                                      */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-close.h"
#include <json-glib/json-glib.h>

G_DEFINE_QUARK (nostrum-msg-close-error-quark, nostrum_msg_close_error)

struct _NostrumMsgClose
{
        gchar *subscription_id; // owned
};

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumMsgClose *
nostrum_msg_close_new (void)
{
        NostrumMsgClose *close = g_new0 (NostrumMsgClose, 1);
        return close;
}

void
nostrum_msg_close_free (NostrumMsgClose *close)
{
        if (!close)
                return;

        g_free (close->subscription_id);
        g_free (close);
}


// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumMsgClose *
nostrum_msg_close_from_json (const char *json, GError **err)
{
        g_return_val_if_fail (json != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        NostrumMsgClose *close = nostrum_msg_close_new ();

        // Parsing JSON string
        g_autoptr (JsonParser) parser = json_parser_new ();
        g_autoptr (GError) json_err = NULL;

        if (!json_parser_load_from_data (parser, json, -1, &json_err)) {
            g_set_error (err,
                         NOSTRUM_MSG_CLOSE_ERROR,
                         NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                         "Error parsing CLOSE JSON: (%s, code=%d): %s",
                         g_quark_to_string (json_err->domain),
                         json_err->code,
                         json_err->message);
            goto error;
        }

        JsonNode *root = json_parser_get_root (parser);
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSE_ERROR,
                             NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                             "CLOSE message must be a JSON array");
                goto error;
        }

        JsonArray *arr = json_node_get_array (root);
        gsize len = json_array_get_length (arr);

        if (len < 2) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSE_ERROR,
                             NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                             "CLOSE message must have at least 2 elements");
                goto error;
        }

        // Element 0: "CLOSE"
        JsonNode *type_node = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (type_node) ||
            json_node_get_value_type (type_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSE_ERROR,
                             NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                             "First element of CLOSE message must be a string");
                goto error;
        }

        const char *type_str = json_node_get_string (type_node);
        if (g_strcmp0 (type_str, "CLOSE") != 0) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSE_ERROR,
                             NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                             "First element of CLOSE message must be " 
                             "\"CLOSE\"");
                goto error;
        }

        // Element 1: subscription id (string)
        JsonNode *id_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE (id_node) ||
            json_node_get_value_type (id_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_CLOSE_ERROR,
                             NOSTRUM_MSG_CLOSE_ERROR_PARSE,
                             "Second element of CLOSE message must be a string "
                             "(subscription id)");
                goto error;
        }

        const char *subs_id_str = json_node_get_string (id_node);
        nostrum_msg_close_set_subscription_id (close, subs_id_str);

        return close; // transfer-full

error:
        nostrum_msg_close_free (close);
        return NULL;
}

gchar *
nostrum_msg_close_to_json (const NostrumMsgClose *close)
{
        g_return_val_if_fail (close != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_array (b);

        // Element 0: "CLOSE"
        json_builder_add_string_value (b, "CLOSE");

        // Element 1: subscription id
        json_builder_add_string_value (b,
                                close->subscription_id ? close->subscription_id
                                                       : "");

        json_builder_end_array (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}

// =============================================================================
// GETTERS
// =============================================================================

const gchar *
nostrum_msg_close_get_subscription_id (const NostrumMsgClose *close)
{
        g_return_val_if_fail (close != NULL, NULL);
        return close->subscription_id;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_msg_close_set_subscription_id (NostrumMsgClose  *close,
                                       const gchar      *subs_id)
{
        g_return_if_fail (close != NULL);

        g_free (close->subscription_id);
        close->subscription_id = subs_id ? g_strdup (subs_id)
                                         : NULL;
}

