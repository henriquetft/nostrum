/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumMsgReq object (Nostr REQ message)                                   */
/* Implementation of nostrum-msg-req.h                                        */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-req.h"
#include "nostrum-json-utils.h"

G_DEFINE_QUARK (nostrum-msg-req-error-quark, nostrum_msg_req_error)

struct _NostrumMsgReq
{
        char       *sub_id;   // owned
        GPtrArray  *filters;  // owned, elements are NostrumFilter*
};


// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumMsgReq *
nostrum_msg_req_new (void)
{
        NostrumMsgReq *req = g_new0 (NostrumMsgReq, 1);

        // Each element is a NostrumFilter*
        req->filters =
          g_ptr_array_new_with_free_func ((GDestroyNotify) nostrum_filter_free);

        return req;
}

void
nostrum_msg_req_free (NostrumMsgReq *req)
{
        if (!req)
                return;

        g_free (req->sub_id);

        if (req->filters)
                g_ptr_array_unref (req->filters);

        g_free (req);
}

// =============================================================================
// OPERATIONS
// =============================================================================

void
nostrum_msg_req_take_one_filter (NostrumMsgReq *req, NostrumFilter *filter)
{
        g_return_if_fail (req != NULL);
        g_return_if_fail (filter != NULL);

        if (!req->filters) {
                req->filters =
                    g_ptr_array_new_with_free_func (
                        (GDestroyNotify) nostrum_filter_free);
        }

        g_ptr_array_add (req->filters, filter);
}

// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumMsgReq *
nostrum_msg_req_from_json_node (const JsonNode *root, GError **err)
{
        g_return_val_if_fail (root != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        NostrumMsgReq *req = nostrum_msg_req_new ();
                
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_set_error (err,
                             NOSTRUM_MSG_REQ_ERROR,
                             NOSTRUM_MSG_REQ_ERROR_PARSE,
                             "REQ message must be a JSON array");
                goto error;
        }

        JsonArray *arr = json_node_get_array (root);
        gsize len = json_array_get_length (arr);

        if (len < 3) {
                g_set_error (err,
                             NOSTRUM_MSG_REQ_ERROR,
                             NOSTRUM_MSG_REQ_ERROR_PARSE,
                             "REQ message must have at least 3 elements");
                goto error;
        }

        // Element 0: "REQ"
        JsonNode *type_node = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (type_node) ||
            json_node_get_value_type (type_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_REQ_ERROR,
                             NOSTRUM_MSG_REQ_ERROR_PARSE,
                             "First element of REQ message must be a string");
                goto error;
        }

        const char *type_str = json_node_get_string (type_node);
        if (g_strcmp0 (type_str, "REQ") != 0) {
                g_set_error (err,
                             NOSTRUM_MSG_REQ_ERROR,
                             NOSTRUM_MSG_REQ_ERROR_PARSE,
                             "First element of REQ message must be \"REQ\"");
                goto error;
        }

        // Element 1: sub_id (string)
        JsonNode *id_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE (id_node) ||
            json_node_get_value_type (id_node) != G_TYPE_STRING) {
                g_set_error (err,
                             NOSTRUM_MSG_REQ_ERROR,
                             NOSTRUM_MSG_REQ_ERROR_PARSE,
                             "Second element of REQ message must be a string "
                             "(subscription id)");
                goto error;
        }

        const char *sub_id_str = json_node_get_string (id_node);

        nostrum_msg_req_set_sub_id (req, sub_id_str);

        // Remaining elements: filters (JSON objects)
        for (guint i = 2; i < len; i++) {
                JsonNode *filter_node = json_array_get_element (arr, i);

                if (!JSON_NODE_HOLDS_OBJECT (filter_node)) {
                        g_set_error (err,
                                     NOSTRUM_MSG_REQ_ERROR,
                                     NOSTRUM_MSG_REQ_ERROR_PARSE,
                                     "Filter element %u of REQ must be a "
                                     "JSON object",
                                     i - 2);
                        goto error;
                }

                g_autoptr(GError) filter_err  = NULL;
                g_autofree gchar *filter_json = json_to_string (filter_node,
                                                                FALSE);
                NostrumFilter *filter =
                    nostrum_filter_from_json (filter_json, &filter_err);

                if (!filter) {
                        g_propagate_error (err, g_steal_pointer (&filter_err));
                        goto error;
                }

                nostrum_msg_req_take_one_filter (req, filter); // takes ownrship
        }

        return req; // transfer-full
error:
        nostrum_msg_req_free (req);
        return NULL;
}


NostrumMsgReq *
nostrum_msg_req_from_json (const char *json, GError **err)
{
        g_return_val_if_fail (json != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        // Parsing JSON string
        g_autoptr(JsonParser) parser = json_parser_new();
        g_autoptr(GError) json_err = NULL;
        if (!json_parser_load_from_data(parser, json, -1, &json_err)) {
                g_set_error(err,
                            NOSTRUM_MSG_REQ_ERROR,
                            NOSTRUM_MSG_REQ_ERROR_PARSE,
                            "Error parsing REQ JSON: (%s, code=%d): %s",
                            g_quark_to_string(json_err->domain),
                            json_err->code,
                            json_err->message);
                return NULL;
        }


        JsonNode *root = json_parser_get_root (parser);

        return nostrum_msg_req_from_json_node(root, err);        
}

gchar *
nostrum_msg_req_to_json (const NostrumMsgReq *req)
{
        g_return_val_if_fail (req != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_array (b);

        // Element 0: "REQ"
        json_builder_add_string_value (b, "REQ");

        // Element 1: subscription id
        json_builder_add_string_value (b, req->sub_id ? req->sub_id : "");

        // Remaining elements: filters (objects)
        if (req->filters) {
                for (guint i = 0; i < req->filters->len; i++) {
                        const NostrumFilter *filter =
                            g_ptr_array_index (req->filters, i);

                        if (!filter) {
                                json_builder_add_null_value (b);
                                continue;
                        }

                        g_autofree gchar *json = nostrum_filter_to_json(filter);

                        g_autoptr(GError) error = NULL;
                        JsonNode *node = json_from_string (json, &error);
                        if (!node) {
                                g_warning ("Failed to parse filter JSON: %s",
                                           error ? error->message
                                                 : "unknown error");
                                json_builder_add_null_value (b);
                                continue;
                        }
                        json_builder_add_value (b, node);
                }
        }


        json_builder_end_array (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}

// =============================================================================
// GETTERS
// =============================================================================

const char *
nostrum_msg_req_get_sub_id (const NostrumMsgReq *req)
{
        g_return_val_if_fail (req != NULL, NULL);
        return req->sub_id;
}

const GPtrArray *
nostrum_msg_req_get_filters (const NostrumMsgReq *req)
{
        g_return_val_if_fail (req != NULL, NULL);
        return req->filters;
}

GPtrArray *
nostrum_msg_req_dup_filters (const NostrumMsgReq *req)
{
        g_return_val_if_fail (req != NULL, NULL);
        const GPtrArray *filters = nostrum_msg_req_get_filters (req);

        GPtrArray *filters_copy =
          g_ptr_array_new_with_free_func ((GDestroyNotify) nostrum_filter_free);

        if (filters) {
                for (guint i = 0; i < filters->len; i++) {
                        const NostrumFilter *f =
                                g_ptr_array_index ((GPtrArray *)filters, i);

                        if (!f)
                                continue;

                        g_ptr_array_add (filters_copy,
                                         nostrum_filter_copy (f));
                }
        }

        return filters_copy;
}


// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_msg_req_set_sub_id (NostrumMsgReq *req, const char *sub_id)
{
        g_return_if_fail (req != NULL);

        g_free (req->sub_id);
        req->sub_id = sub_id ? g_strdup (sub_id)
                             : NULL;
}

void
nostrum_msg_req_take_filters (NostrumMsgReq *req, GPtrArray *filters)
{
        g_return_if_fail (req != NULL);

        if (req->filters)
                g_ptr_array_unref (req->filters);

        req->filters = filters;
}
