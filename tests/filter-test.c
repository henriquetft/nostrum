/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumFilter                                                    */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-event.h"
#include "nostrum-filter.h"
#include "nostrum-utils.h"
#include <json-glib/json-glib.h>
#include <glib.h>


static NostrumEvent *
make_base_event (void)
{
        NostrumEvent *e = nostrum_event_new ();
        nostrum_event_set_id (e, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        nostrum_event_set_pubkey (e, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        nostrum_event_set_created_at (e, 1672531200L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "hello");

        nostrum_event_add_tag (e, "e",
                               "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
                               "wss://relay.example.com",
                               NULL);
        nostrum_event_add_tag (e, "p",
                               "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
                               NULL);
        nostrum_event_add_tag (e, "t", "nostr", NULL);

        return e;
}



static void
test_matches_ids_exact (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids_ok[] = {
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", NULL
                };
                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids_ok));
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids_bad[] = {
                        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", NULL
                };
                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids_bad));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_ids_prefix (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids[] = { "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", NULL };
                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids));
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids_pref_bad[] = { "aaa", NULL };
                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids_pref_bad));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_authors_exact_and_prefix (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *auth_bad[] = {
                        "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", NULL
                };
                nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (auth_bad));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *auth_ok[] = {
                        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", NULL
                };
                nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (auth_ok));
                g_assert_true (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_kinds (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const int kinds1[] = { 7, 1, 42 };
                nostrum_filter_take_kinds (f, nostrum_utils_new_int_garray (kinds1, G_N_ELEMENTS (kinds1)));
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const int kinds2[] = { 2, 3, 4 };
                nostrum_filter_take_kinds (f, nostrum_utils_new_int_garray (kinds2, G_N_ELEMENTS (kinds2)));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_since_until (void)
{
         // created_at: 1672531200
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                nostrum_filter_set_since (f, 1672530000L);
                nostrum_filter_set_until (f, 1672531300L);
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                nostrum_filter_set_since (f, 1672531201L);
                g_assert_false (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                nostrum_filter_set_until (f, 1672531199L);
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_tags_single_key_or (void)
{
        // Tags: ["e","cccc..."], ["p","dddd..."], ["t","nostr"]
        g_autoptr (NostrumEvent) e = make_base_event ();

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *e_vals_ok[] = {
                        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
                        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                        NULL
                };
                nostrum_filter_take_tags (f, nostrum_utils_new_table_str_to_ptrarray ("e", e_vals_ok));
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *e_vals_bad[] = { "eeee", "ffff", NULL };
                nostrum_filter_take_tags (f, nostrum_utils_new_table_str_to_ptrarray ("e", e_vals_bad));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_tags_multiple_keys_and (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        // Requires #e and #p
        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();

                GHashTable *ht = g_hash_table_new_full (
                    g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_ptr_array_unref);
                const char *e_vals[] = {
                        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc", NULL
                };
                const char *p_vals[] = {
                        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", NULL
                };
                g_hash_table_insert (ht, g_strdup ("e"), nostrum_utils_new_str_ptrarray (e_vals));
                g_hash_table_insert (ht, g_strdup ("p"), nostrum_utils_new_str_ptrarray (p_vals));

                nostrum_filter_take_tags (f, ht); // ownership -> filter
                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *p_vals_bad[] = {
                        "pppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppppp", NULL
                };
                nostrum_filter_take_tags (f, nostrum_utils_new_table_str_to_ptrarray ("p", p_vals_bad));
                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}

static void
test_matches_combined_all (void)
{
        g_autoptr (NostrumEvent) e = make_base_event ();

        // OK
        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids[] = { "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", NULL };
                const char *auth[] = { "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", NULL };
                const int kinds[] = { 1 };

                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids));
                nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (auth));
                nostrum_filter_take_kinds (f, nostrum_utils_new_int_garray (kinds, 1));
                nostrum_filter_set_since (f, 1672530000L);
                nostrum_filter_set_until (f, 1672531300L);
                nostrum_filter_take_tags (f, nostrum_utils_new_table_str_to_ptrarray ("t", (const char *[]) { "nostr", NULL }));

                g_assert_true (nostrum_filter_matches_event (f, e));
        }

        {
                g_autoptr (NostrumFilter) f = nostrum_filter_new ();
                const char *ids[] = { "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", NULL };
                const char *auth[] = { "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", NULL };
                const int kinds[] = { 1 };

                nostrum_filter_take_ids (f, nostrum_utils_new_str_ptrarray (ids));
                nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (auth));
                nostrum_filter_take_kinds (f, nostrum_utils_new_int_garray (kinds, 1));
                nostrum_filter_set_since (f, 1672530000L);
                nostrum_filter_set_until (f, 1672531300L);
                nostrum_filter_take_tags (f, nostrum_utils_new_table_str_to_ptrarray ("t", (const char *[]) { "other", NULL }));

                g_assert_false (nostrum_filter_matches_event (f, e));
        }
}



static void
test_from_json_ok (void)
{
        static const char *SAMPLE_JSON =
                "{"
                "  \"ids\": [\"id1\", \"id2\"],"
                "  \"authors\": [\"pub1\", \"pub2\"],"
                "  \"kinds\": [1, 7],"
                "  \"since\": 100,"
                "  \"until\": 200,"
                "  \"limit\": 10,"
                "  \"#e\": [\"ev1\", \"ev2\"],"
                "  \"#p\": [\"pk1\"]"
                "}";

        g_autoptr(GError) err = NULL;

        g_autoptr (NostrumFilter) f =
            nostrum_filter_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (f);

        // ids
        const GPtrArray *ids = nostrum_filter_get_ids (f);
        g_assert_nonnull (ids);
        g_assert_cmpint ((int) ids->len, ==, 2);

        const char *id0 = g_ptr_array_index (ids, 0);
        const char *id1 = g_ptr_array_index (ids, 1);
        g_assert_cmpstr (id0, ==, "id1");
        g_assert_cmpstr (id1, ==, "id2");

        // authors
        const GPtrArray *authors = nostrum_filter_get_authors (f);
        g_assert_nonnull (authors);
        g_assert_cmpint ((int) authors->len, ==, 2);

        const char *a0 = g_ptr_array_index ((GPtrArray *) authors, 0);
        const char *a1 = g_ptr_array_index ((GPtrArray *) authors, 1);
        g_assert_cmpstr (a0, ==, "pub1");
        g_assert_cmpstr (a1, ==, "pub2");

        // kinds
        const GArray *kinds = nostrum_filter_get_kinds (f);
        g_assert_nonnull (kinds);
        g_assert_cmpint ((int) kinds->len, ==, 2);

        gint k0 = g_array_index ((GArray *) kinds, gint, 0);
        gint k1 = g_array_index ((GArray *) kinds, gint, 1);
        g_assert_cmpint (k0, ==, 1);
        g_assert_cmpint (k1, ==, 7);

        // since / until / limit
        g_assert_cmpint ((int) nostrum_filter_get_since (f), ==, 100);
        g_assert_cmpint ((int) nostrum_filter_get_until (f), ==, 200);
        g_assert_cmpint (nostrum_filter_get_limit (f), ==, 10);

        // tags
        GHashTable *tags = nostrum_filter_get_tags (f);
        g_assert_nonnull (tags);

        const GPtrArray *tag_e =
                g_hash_table_lookup (tags, "#e");
        g_assert_nonnull (tag_e);
        g_assert_cmpint ((int) tag_e->len, ==, 2);
        const char *e0 = g_ptr_array_index (tag_e, 0);
        const char *e1 = g_ptr_array_index (tag_e, 1);
        g_assert_cmpstr (e0, ==, "ev1");
        g_assert_cmpstr (e1, ==, "ev2");

        const GPtrArray *tag_p = g_hash_table_lookup (tags, "#p");
        g_assert_nonnull (tag_p);
        g_assert_cmpint ((int) tag_p->len, ==, 1);
        const char *p0 = g_ptr_array_index (tag_p, 0);
        g_assert_cmpstr (p0, ==, "pk1");
}



static void
test_to_json_ok (void)
{
        g_autoptr (NostrumFilter) f = nostrum_filter_new ();

        // ids
        GPtrArray *ids = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (ids, g_strdup ("id1"));
        g_ptr_array_add (ids, g_strdup ("id2"));
        nostrum_filter_take_ids (f, ids);

        // authors
        GPtrArray *authors = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (authors, g_strdup ("pub1"));
        g_ptr_array_add (authors, g_strdup ("pub2"));
        nostrum_filter_take_authors (f, authors);

        // kinds
        GArray *kinds = g_array_new (FALSE, FALSE, sizeof (gint));
        gint k1 = 1;
        gint k7 = 7;
        g_array_append_val (kinds, k1);
        g_array_append_val (kinds, k7);
        nostrum_filter_take_kinds (f, kinds);

        // since / until / limit
        nostrum_filter_set_since (f, 100);
        nostrum_filter_set_until (f, 200);
        nostrum_filter_set_limit (f, 10);

        // tags
        GHashTable *tags = g_hash_table_new_full (
            g_str_hash,
            g_str_equal,
            g_free,
            (GDestroyNotify) g_ptr_array_unref);

        GPtrArray *tag_e = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (tag_e, g_strdup ("ev1"));
        g_ptr_array_add (tag_e, g_strdup ("ev2"));
        g_hash_table_insert (tags, g_strdup ("#e"), tag_e);

        GPtrArray *tag_p = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (tag_p, g_strdup ("pk1"));
        g_hash_table_insert (tags, g_strdup ("#p"), tag_p);

        nostrum_filter_take_tags (f, tags);

        // to_json
        g_autofree gchar *json = nostrum_filter_to_json (f);
        g_assert_nonnull (json);

        // from_json to check all attributes
        g_autoptr(GError) err = NULL;
        g_autoptr (NostrumFilter) f2 = nostrum_filter_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (f2);

        // Checking all attributes ...

        // ids
        const GPtrArray *ids2 = nostrum_filter_get_ids (f2);
        g_assert_nonnull (ids2);
        g_assert_cmpint ((int) ids2->len, ==, 2);
        const char *id0 = g_ptr_array_index ((GPtrArray *) ids2, 0);
        const char *id1 = g_ptr_array_index ((GPtrArray *) ids2, 1);
        g_assert_cmpstr (id0, ==, "id1");
        g_assert_cmpstr (id1, ==, "id2");

        // authors
        const GPtrArray *authors2 = nostrum_filter_get_authors (f2);
        g_assert_nonnull (authors2);
        g_assert_cmpint ((int) authors2->len, ==, 2);
        const char *a0 = g_ptr_array_index ((GPtrArray *) authors2, 0);
        const char *a1 = g_ptr_array_index ((GPtrArray *) authors2, 1);
        g_assert_cmpstr (a0, ==, "pub1");
        g_assert_cmpstr (a1, ==, "pub2");

        // kinds
        const GArray *kinds2 = nostrum_filter_get_kinds (f2);
        g_assert_nonnull (kinds2);
        g_assert_cmpint ((int) kinds2->len, ==, 2);
        gint kk0 = g_array_index ((GArray *) kinds2, gint, 0);
        gint kk1 = g_array_index ((GArray *) kinds2, gint, 1);
        g_assert_cmpint (kk0, ==, 1);
        g_assert_cmpint (kk1, ==, 7);

        // since / until / limit
        g_assert_cmpint ((int) nostrum_filter_get_since (f2), ==, 100);
        g_assert_cmpint ((int) nostrum_filter_get_until (f2), ==, 200);
        g_assert_cmpint (nostrum_filter_get_limit (f2), ==, 10);

        // tags
        GHashTable *tags2 = nostrum_filter_get_tags (f2);
        g_assert_nonnull (tags2);

        const GPtrArray *tag_e2 = g_hash_table_lookup (tags2, "#e");
        g_assert_nonnull (tag_e2);
        g_assert_cmpint ((int) tag_e2->len, ==, 2);
        const char *e0 = g_ptr_array_index (tag_e2, 0);
        const char *e1 = g_ptr_array_index (tag_e2, 1);
        g_assert_cmpstr (e0, ==, "ev1");
        g_assert_cmpstr (e1, ==, "ev2");

        const GPtrArray *tag_p2 = g_hash_table_lookup (tags2, "#p");
        g_assert_nonnull (tag_p2);
        g_assert_cmpint ((int) tag_p2->len, ==, 1);
        const char *p0 = g_ptr_array_index (tag_p2, 0);
        g_assert_cmpstr (p0, ==, "pk1");
}

static void
test_from_json_error_parsing_json (void)
{
        static const char *BAD_JSON = "{ \"ids\": [\"a\" }";

        GError *err = NULL;
        NostrumFilter *f = nostrum_filter_from_json (BAD_JSON, &err);

        g_assert_null (f);
        g_assert_error (err,
                        NOSTRUM_FILTER_ERROR,
                        NOSTRUM_FILTER_ERROR_PARSE);

        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing filter JSON") != NULL);

        g_clear_error (&err);
}

static void
test_from_json_error_non_object_root (void)
{
        static const char *BAD_JSON = "[\"not\", \"an\", \"object\"]";

        GError *err = NULL;
        NostrumFilter *f = nostrum_filter_from_json (BAD_JSON, &err);

        g_assert_null (f);
        g_assert_error (err,
                        NOSTRUM_FILTER_ERROR,
                        NOSTRUM_FILTER_ERROR_PARSE);

        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Filter JSON must be an object") != NULL);

        g_clear_error (&err);
}


int
main (int argc, char *argv[])
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/filter/matches_ids_exact", test_matches_ids_exact);
        g_test_add_func ("/filter/matches_ids_prefix", test_matches_ids_prefix);
        g_test_add_func ("/filter/matches_authors_exact_prefix", test_matches_authors_exact_and_prefix);
        g_test_add_func ("/filter/matches_kinds", test_matches_kinds);
        g_test_add_func ("/filter/matches_since_until", test_matches_since_until);
        g_test_add_func ("/filter/matches_tags_single_or", test_matches_tags_single_key_or);
        g_test_add_func ("/filter/matches_tags_multi_and", test_matches_tags_multiple_keys_and);
        g_test_add_func ("/filter/matches_combined", test_matches_combined_all);

        g_test_add_func ("/filter/to_json_ok", test_to_json_ok);
        g_test_add_func ("/filter/from_json_ok", test_from_json_ok);

        g_test_add_func ("/filter/from_json_error_parsing_json",
                         test_from_json_error_parsing_json);
        g_test_add_func ("/filter/from_json_error_non_object_root",
                         test_from_json_error_non_object_root);

        return g_test_run ();
}
