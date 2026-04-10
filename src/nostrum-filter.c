/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumFilter object (implementation of nostrum-filter.h)                  */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */


#include "nostrum-filter.h"
#include "nostrum-event.h"
#include "nostrum-json-utils.h"
#include "nostrum-utils.h"
#include <glib.h>
#include <json-glib/json-glib.h>


struct _NostrumFilter
{
        GPtrArray   *ids;     // char* (hex),                  g_free
        GPtrArray   *authors; // char* (hex),                  g_free
        GArray      *kinds;   // array of gint
        gint64       since;   // -1 = absent
        gint64       until;   // -1 = absent
        GHashTable  *tags;    // char* -> GPtrArray*(char*);   g_ptr_array_unref
        GPtrArray   *dedup_keys; // char*,                     g_free
        gint         limit;   // <=0 = no limit
};

G_DEFINE_QUARK (nostrum-filter-error-quark, nostrum_filter_error)

// =============================================================================
// HELPERS -- DECLARATIONS
// =============================================================================

static gboolean match_kinds          (const NostrumFilter    *filter,
                                      gint                    kind);

static gboolean array_has_element    (const GPtrArray        *arr,
                                      const gchar            *value);

static gboolean match_ids            (const NostrumFilter    *filter,
                                      const gchar            *id);

static gboolean match_tags           (const NostrumFilter    *filter,
                                      const GPtrArray        *tags);

static gboolean match_authors        (const NostrumFilter    *filter,
                                      const gchar            *pubkey);

static gboolean match_dedup_keys     (const NostrumFilter    *filter,
                                      const gchar            *dedup_key);

static gboolean match_created_at     (const NostrumFilter    *filter,
                                      gint64                  created_at);

static gboolean match_one_tag_filter (const GPtrArray        *event_tags,
                                      const gchar            *f_tag_name,
                                      const GPtrArray        *f_tag_values);

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumFilter *
nostrum_filter_new (void)
{
        NostrumFilter *f = g_new0 (NostrumFilter, 1);
        f->since = -1;
        f->until = -1;
        f->limit = -1;
        return f;
}

void
nostrum_filter_free (NostrumFilter *f)
{
        if (!f) {
                return;
        }

        if (f->ids) {
                g_ptr_array_unref (f->ids);
                f->ids = NULL;
        }
        if (f->authors) {
                g_ptr_array_unref (f->authors);
                f->authors = NULL;
        }
        if (f->kinds) {
                g_array_unref (f->kinds);
                f->kinds = NULL;
        }
        if (f->tags) {
                g_hash_table_unref (f->tags);
                f->tags = NULL;
        }
        if (f->dedup_keys) {
                g_ptr_array_unref (f->dedup_keys);
                f->dedup_keys = NULL;
        }

        g_free (f);
}

NostrumFilter *
nostrum_filter_copy (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);

        NostrumFilter *c = nostrum_filter_new ();

        // scalars
        c->since = f->since;
        c->until = f->until;
        c->limit = f->limit;

        // ids/authors: deep copy strings
        if (f->ids && f->ids->len > 0)
                c->ids = nostrum_utils_dup_str_ptr_array (f->ids);

        if (f->authors && f->authors->len > 0)
                c->authors = nostrum_utils_dup_str_ptr_array (f->authors);

        // kinds: deep copy ints
        if (f->kinds && f->kinds->len > 0)
                c->kinds = nostrum_utils_dup_int_garray (f->kinds);

        // tags: deep copy hashtable key + arrays + strings
        if (f->tags && g_hash_table_size (f->tags) > 0) {
                c->tags = g_hash_table_new_full (
                  g_str_hash,
                  g_str_equal,
                  g_free,     // free key string
                  (GDestroyNotify)g_ptr_array_unref);

                GHashTableIter iter;
                gpointer key, value;

                g_hash_table_iter_init (&iter, (GHashTable *) f->tags);
                while (g_hash_table_iter_next (&iter, &key, &value)) {
                        const gchar *k = key;             // "#e", "#p", ...
                        const GPtrArray *vals = value;    // array of char*

                        if (!k || !vals)
                                continue;

                        gchar *k_copy = g_strdup (k);
                        GPtrArray *vals_copy =
                          nostrum_utils_dup_str_ptr_array (vals);

                        g_hash_table_insert (c->tags, k_copy, vals_copy);
                }
        }

        if (f->dedup_keys && f->dedup_keys->len > 0)
                c->dedup_keys = nostrum_utils_dup_str_ptr_array (f->dedup_keys);

        return c;
}

// =============================================================================
// OPERATIONS
// =============================================================================

gboolean
nostrum_filter_matches_event (const NostrumFilter  *filter,
                              const NostrumEvent   *event)
{
        // Events must match all conditions within a single filter object (AND)
        const gchar      *id         = nostrum_event_get_id (event);
        const gchar      *pubkey     = nostrum_event_get_pubkey (event);
        const gint        kind       = nostrum_event_get_kind (event);
        const gint64      created_at = nostrum_event_get_created_at (event);
        const GPtrArray  *tags       = nostrum_event_get_tags (event);
        const gchar      *dedup_keys = nostrum_event_get_dedup_key (event);

        if (!match_ids (filter, id))
                return FALSE;
        if (!match_authors (filter, pubkey))
                return FALSE;
        if (!match_kinds (filter, kind))
                return FALSE;
        if (!match_created_at (filter, created_at))
                return FALSE;
        if (!match_tags (filter, tags))
                return FALSE;
        if (!match_dedup_keys (filter, dedup_keys))
                return FALSE;
        
        return TRUE;
}

gboolean
nostrum_filter_is_empty (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, TRUE);

        const GPtrArray  *ids        = nostrum_filter_get_ids (f);
        const GPtrArray  *authors    = nostrum_filter_get_authors (f);
        const GArray     *kinds      = nostrum_filter_get_kinds (f);
        gint64            since      = nostrum_filter_get_since (f);
        gint64            until      = nostrum_filter_get_until (f);
        const GHashTable *tags       = nostrum_filter_get_tags (f);
        const GPtrArray  *dedup_keys = nostrum_filter_get_dedup_keys (f);

        gboolean has_filter =
               (ids && ids->len > 0) ||
               (authors && authors->len > 0) ||
               (kinds && kinds->len > 0) ||
               (since >= 0) ||
               (until >= 0) ||
               (tags && g_hash_table_size (tags) > 0) ||
               (dedup_keys && dedup_keys->len > 0);

        return !has_filter;

}

// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumFilter *
nostrum_filter_from_json (const char *json_str, GError **err)
{
        // Preconditions
        g_return_val_if_fail (json_str != NULL,            NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        g_autoptr (GList) members = NULL;

        NostrumFilter *f = nostrum_filter_new ();

        // Parsing JSON string
        g_autoptr(JsonParser) p = json_parser_new();
        g_autoptr(GError) json_err = NULL;
        if (!json_parser_load_from_data(p, json_str, -1, &json_err)) {
                g_set_error(err,
                            NOSTRUM_FILTER_ERROR,
                            NOSTRUM_FILTER_ERROR_PARSE,
                            "Error parsing filter JSON: (%s, code=%d): %s",
                            g_quark_to_string(json_err->domain),
                            json_err->code,
                            json_err->message);
                goto error;
        }

        JsonNode *node = json_parser_get_root (p);

        if (!JSON_NODE_HOLDS_OBJECT (node)) {
                g_set_error (err,
                             NOSTRUM_FILTER_ERROR,
                             NOSTRUM_FILTER_ERROR_PARSE,
                             "Filter JSON must be an object");
                goto error;
        }

        JsonObject *fo = json_node_get_object (node);

        members = json_object_get_members (fo);

        for (GList *l = members; l; l = l->next) {
                const gchar *key = l->data;
                JsonNode *val = json_object_get_member (fo, key);

                 if (g_strcmp0 (key, "ids") == 0) {
                        GPtrArray *ids =
                          nostrum_json_utils_parse_str_array (val);
                        if (ids) {
                                nostrum_filter_take_ids (f, ids);
                        }

                 } else if (g_strcmp0 (key, "authors") == 0) {
                        GPtrArray *authors =
                          nostrum_json_utils_parse_str_array (val);
                        if (authors) {
                                nostrum_filter_take_authors (f, authors);
                        }

                 } else if (g_strcmp0 (key, "kinds") == 0) {
                        GArray *kinds = nostrum_json_utils_parse_int_array(val);
                        if (kinds) {
                                nostrum_filter_take_kinds (f, kinds);
                        }
                        
                 } else if (g_strcmp0 (key, "since") == 0) {
                        if (JSON_NODE_HOLDS_VALUE (val)) {
                                gint64 since = json_node_get_int (val);
                                nostrum_filter_set_since (f, since);
                        }

                 } else if (g_strcmp0 (key, "until") == 0) {
                        if (JSON_NODE_HOLDS_VALUE (val)) {
                                gint64 until = json_node_get_int (val);
                                nostrum_filter_set_until (f, until);
                        }
                } else if (g_strcmp0 (key, "limit") == 0) {
                        if (JSON_NODE_HOLDS_VALUE (val)) {
                                gint64 lmt = json_node_get_int (val);
                                nostrum_filter_set_limit (f, (gint)lmt);
                        }

                 } else if (key[0] == '#') {
                        // FIXME handle tags here, not after
                        continue;
                 } else {
                        g_set_error (err,
                                     NOSTRUM_FILTER_ERROR,
                                     NOSTRUM_FILTER_UNKNOWN_ELEMENT,
                                     "Unknown element of filter");
                        goto error;
                 }
        }


        // tags "#x" -> array of strings
        GHashTable *tags = nostrum_json_utils_parse_tags (fo);
        if (g_hash_table_size (tags) > 0) {
                nostrum_filter_take_tags (f, tags);
        } else {
                g_hash_table_unref (tags);
        }

        return f;
error:
        nostrum_filter_free (f);
        return NULL;
}


gchar *
nostrum_filter_to_json (const NostrumFilter *filter)
{
        g_return_val_if_fail (filter != NULL, NULL);

        g_autoptr (JsonBuilder) b = json_builder_new ();

        json_builder_begin_object (b);

        // ---------------------------------------------------------------------
        // ids: array of strings
        // ---------------------------------------------------------------------
        if (filter->ids && filter->ids->len > 0) {
                json_builder_set_member_name (b, "ids");
                json_builder_begin_array (b);

                for (guint i = 0; i < filter->ids->len; i++) {
                        const char *id = g_ptr_array_index (filter->ids, i);
                        if (id != NULL)
                                json_builder_add_string_value (b, id);
                }

                json_builder_end_array (b);
        }

        // ---------------------------------------------------------------------
        // authors: array of strings
        // ---------------------------------------------------------------------
        if (filter->authors && filter->authors->len > 0) {
                json_builder_set_member_name (b, "authors");
                json_builder_begin_array (b);

                for (guint i = 0; i < filter->authors->len; i++) {
                        const char *author = g_ptr_array_index (filter->authors,
                                                                i);
                        if (author != NULL)
                                json_builder_add_string_value (b, author);
                }

                json_builder_end_array (b);
        }

        // ---------------------------------------------------------------------
        // kinds: array of integers
        // ---------------------------------------------------------------------
        if (filter->kinds && filter->kinds->len > 0) {
                json_builder_set_member_name (b, "kinds");
                json_builder_begin_array (b);

                for (guint i = 0; i < filter->kinds->len; i++) {
                        gint kind = g_array_index (filter->kinds, gint, i);
                        json_builder_add_int_value (b, kind);
                }

                json_builder_end_array (b);
        }

        // ---------------------------------------------------------------------
        // since / until / limit
        // since, until: -1 means "absent"
        // limit: <= 0 means "no limit"
        // ---------------------------------------------------------------------
        if (filter->since >= 0) {
                json_builder_set_member_name (b, "since");
                json_builder_add_int_value (b, filter->since);
        }

        if (filter->until >= 0) {
                json_builder_set_member_name (b, "until");
                json_builder_add_int_value (b, filter->until);
        }

        if (filter->limit > -1) {
                json_builder_set_member_name (b, "limit");
                json_builder_add_int_value (b, filter->limit);
        }

        // ---------------------------------------------------------------------
        // tags: keys like "#e", "#p", "#r", etc.
        // tags: GHashTable *tags; key: char* -> val: GPtrArray*(char*)
        // ---------------------------------------------------------------------
        if (filter->tags && g_hash_table_size (filter->tags) > 0) {
                GHashTableIter iter;
                gpointer key, value;

                g_hash_table_iter_init (&iter, filter->tags);

                while (g_hash_table_iter_next (&iter, &key, &value)) {
                        const char  *tag_name = key;   // e.g. "#e"
                        GPtrArray   *vals     = value; // array of char*

                        if (!tag_name || !vals || vals->len == 0)
                                continue;

                        json_builder_set_member_name (b, tag_name);
                        json_builder_begin_array (b);

                        for (guint i = 0; i < vals->len; i++) {
                                const char *s = g_ptr_array_index (vals, i);
                                if (s != NULL)
                                        json_builder_add_string_value (b, s);
                        }

                        json_builder_end_array (b);
                }
        }

        json_builder_end_object (b);

        g_autoptr (JsonNode) root = json_builder_get_root (b);
        return json_to_string (root, FALSE); // transfer-full
}


// =============================================================================
// GETTERS
// =============================================================================

const GPtrArray *
nostrum_filter_get_ids (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);
        return f->ids;
}

const GPtrArray *
nostrum_filter_get_authors (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);
        return f->authors;
}

const GArray *
nostrum_filter_get_kinds (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);
        return f->kinds;
}

gint64
nostrum_filter_get_since (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, -1);
        return f->since;
}

gint64
nostrum_filter_get_until (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, -1);
        return f->until;
}

const GHashTable *
nostrum_filter_get_tags (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);
        return f->tags;
}

gint
nostrum_filter_get_limit (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, 0);
        return f->limit;
}

const GPtrArray *
nostrum_filter_get_dedup_keys (const NostrumFilter *f)
{
        g_return_val_if_fail (f != NULL, NULL);
        return f->dedup_keys;
}

// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_filter_take_ids (NostrumFilter *f, GPtrArray *ids)
{
        g_return_if_fail (f != NULL);
        if (f->ids)
                g_ptr_array_unref (f->ids);
        f->ids = ids;
}

void
nostrum_filter_take_authors (NostrumFilter *f, GPtrArray *authors)
{
        g_return_if_fail (f != NULL);
        if (f->authors)
                g_ptr_array_unref (f->authors);
        f->authors = authors;
}

void
nostrum_filter_take_kinds (NostrumFilter *f, GArray *kinds)
{
        g_return_if_fail (f != NULL);
        if (f->kinds)
                g_array_unref (f->kinds);
        f->kinds = kinds;
}

void
nostrum_filter_set_since (NostrumFilter *f, gint64 since)
{
        g_return_if_fail (f != NULL);
        f->since = since;
}

void
nostrum_filter_set_until (NostrumFilter *f, gint64 until)
{
        g_return_if_fail (f != NULL);
        f->until = until;
}

void
nostrum_filter_take_tags (NostrumFilter *f, GHashTable *tags)
{
        g_return_if_fail (f != NULL);
        if (f->tags)
                g_hash_table_unref (f->tags);
        f->tags = tags; 
}

void
nostrum_filter_set_limit (NostrumFilter *f, gint limit)
{
        g_return_if_fail (f != NULL);
        f->limit = limit;
}


void
nostrum_filter_take_dedup_keys (NostrumFilter *f, GPtrArray *dedup_keys)
{
        g_return_if_fail (f != NULL);
        if (f->dedup_keys)
                g_ptr_array_unref (f->dedup_keys);
        f->dedup_keys = dedup_keys;
}

// =============================================================================
// HELPERS -- IMPLEMENTATION
// =============================================================================

static gboolean
match_created_at (const NostrumFilter *filter, gint64 created_at)
{
        const gint64 since = nostrum_filter_get_since (filter);
        const gint64 until = nostrum_filter_get_until (filter);

        if (since != -1 && created_at < since) {
                return FALSE;
        }

        if (until != -1 && created_at > until) {
                return FALSE;
        }

        return TRUE;
}

static gboolean
match_ids (const NostrumFilter *filter, const gchar *id)
{
        const GPtrArray *ids = nostrum_filter_get_ids (filter);
        if (!ids || ids->len == 0) {
                return TRUE;
        }
        return array_has_element (ids, id);
}

static gboolean
match_authors (const NostrumFilter *filter, const gchar *pubkey)
{
        const GPtrArray *authors = nostrum_filter_get_authors (filter);
        if (!authors || authors->len == 0) {
                return TRUE;
        }

        return array_has_element (authors, pubkey);
}

static gboolean
match_kinds (const NostrumFilter *filter, gint kind)
{
        const GArray *f_kinds = nostrum_filter_get_kinds (filter);
        if (!f_kinds || f_kinds->len == 0) {
                return TRUE;
        }

        for (guint i = 0; i < f_kinds->len; i++) {
                gint k = g_array_index ((GArray *)f_kinds, gint, i);
                if (k == kind) {
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
match_dedup_keys (const NostrumFilter *filter, const gchar *dedup_key)
{
        const GPtrArray *dedup_keys = nostrum_filter_get_dedup_keys (filter);
        if (!dedup_keys || dedup_keys->len == 0) {
                return TRUE;
        }

        return array_has_element (dedup_keys, dedup_key);
}


static gboolean
array_has_element (const GPtrArray *arr, const gchar *value)
{
        if (!value) {
                return FALSE;
        }

        for (guint i = 0; i < arr->len; i++) {
                const gchar *s = g_ptr_array_index ((GPtrArray *)arr, i);
                if (s && g_strcmp0 (s, value) == 0)
                        return TRUE;
        }
        return FALSE;
}

static gboolean
match_tags (const NostrumFilter *filter, const GPtrArray *event_tags)
{
        const GHashTable *filter_tags = nostrum_filter_get_tags (filter);

        if (!filter_tags || g_hash_table_size ((GHashTable *)filter_tags) == 0)
                return TRUE;

        GHashTableIter it;
        gpointer k, v;
        g_hash_table_iter_init (&it, (GHashTable *)filter_tags);

        // Iterate over filters. All tags must match
        while (g_hash_table_iter_next (&it, &k, &v)) {
                // "#e", "#p", ....
                const gchar *f_tag_key = (const gchar *)k;
                const GPtrArray *f_tag_values = (const GPtrArray *)v;

                const gchar *f_tag_name =
                    (f_tag_key && f_tag_key[0] == '#') ? f_tag_key + 1
                                                       : f_tag_key;

                if (g_strcmp0 (f_tag_name, "e") == 0 ||
                    g_strcmp0 (f_tag_name, "p") == 0) {
                        if (!match_one_tag_filter (event_tags,
                                                   f_tag_name,
                                                   f_tag_values))
                                return FALSE;
                } else {
                        if (!match_one_tag_filter (event_tags,
                                                   f_tag_name,
                                                   f_tag_values))
                                return FALSE;
                }
        }
        return TRUE;
}


static gboolean
match_one_tag_filter (const GPtrArray    *event_tags,
                      const gchar        *f_tag_name,
                      const GPtrArray    *f_tag_values)
{
        if (!f_tag_values || f_tag_values->len == 0)
                return TRUE;
        if (!event_tags || event_tags->len == 0)
                return FALSE;

        for (guint i = 0; i < event_tags->len; i++) {
                const GPtrArray *e_tag =
                    g_ptr_array_index ((GPtrArray *)event_tags, i);
                // tag name and tag value
                if (!e_tag || e_tag->len < 2)
                        continue;

                const gchar *e_tag_name =
                    g_ptr_array_index ((GPtrArray *)e_tag, 0);
                if (!e_tag_name || g_strcmp0 (e_tag_name, f_tag_name) != 0) {
                        continue;
                }

                for (guint w = 0; w < f_tag_values->len; w++) {
                        const gchar *want =
                            g_ptr_array_index ((GPtrArray *)f_tag_values, w);
                        if (!want) {
                                continue;
                        }

                        const gchar *val =
                            g_ptr_array_index ((GPtrArray *)e_tag, 1);
                        if (val && g_strcmp0 (val, want) == 0)
                                return TRUE;
                }
        }

        return FALSE;
}
