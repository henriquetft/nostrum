/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* JSON helper functions. (Impl. of nostrum-json-utils.h)                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-json-utils.h"
#include "nostrum-filter.h"

GPtrArray *
nostrum_json_utils_parse_str_array (JsonNode *node)
{
        if (!JSON_NODE_HOLDS_ARRAY (node)) {
                return NULL;
        }

        JsonArray *ja = json_node_get_array (node);
        gsize n = json_array_get_length (ja);

        GPtrArray *out = g_ptr_array_new_with_free_func (g_free);
        for (gsize i = 0; i < n; i++) {
                JsonNode *elem = json_array_get_element (ja, i);
                if (JSON_NODE_HOLDS_VALUE (elem) &&
                    json_node_get_value_type (elem) == G_TYPE_STRING) {
                        const gchar *s = json_node_get_string (elem);
                        g_ptr_array_add (out, g_strdup (s ? s : ""));
                }
        }
        return out;
}


GArray *
nostrum_json_utils_parse_int_array (JsonNode *node)
{
        if (!JSON_NODE_HOLDS_ARRAY (node)) {
                return NULL;
        }
        JsonArray *ja = json_node_get_array (node);
        gsize n = json_array_get_length (ja);

        GArray *out = g_array_new (FALSE, FALSE, sizeof (gint));
        for (gsize i = 0; i < n; i++) {
                JsonNode *elem = json_array_get_element (ja, i);
                if (JSON_NODE_HOLDS_VALUE (elem) &&
                   (json_node_get_value_type (elem) == G_TYPE_INT64 ||
                    json_node_get_value_type (elem) == G_TYPE_INT)) {
                        gint64 v = json_node_get_int (elem);
                        gint vv = (gint)v;
                        g_array_append_val (out, vv);
                }
        }
        return out;
}


GHashTable *
nostrum_json_utils_parse_tags (JsonObject *obj)
{
        GHashTable *tags =
            g_hash_table_new_full (g_str_hash,
                                   g_str_equal,
                                   g_free,
                                   (GDestroyNotify)g_ptr_array_unref);

        GList *members = json_object_get_members (obj);
        for (GList *l = members; l != NULL; l = l->next) {
                const gchar *key = l->data;
                if (key && key[0] == '#') {
                        JsonNode *val = json_object_get_member (obj, key);
                        GPtrArray *arr =
                            nostrum_json_utils_parse_str_array (val);
                        if (arr) {
                                g_hash_table_insert (tags, g_strdup (key), arr);
                        }
                }
        }
        g_list_free (members);

        return tags;
}


char *
nostrum_json_utils_node_to_str (JsonNode *node)
{
        g_autoptr (JsonGenerator) gen = json_generator_new ();
        json_generator_set_root (gen, node);
        json_generator_set_pretty (gen, FALSE);
        return json_generator_to_data (gen, NULL);
}
