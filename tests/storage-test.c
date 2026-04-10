/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumStorage                                                   */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-event.h"
#include "nostrum-filter.h"
#include "nostrum-storage.h"
#include "nostrum-utils.h"

#include <glib.h>
#include <glib/gstdio.h>

typedef struct
{
        gchar             *db_path;
        NostrumStorage    *storage;
} StorageFixture;


static gchar *
make_tmp_db_path (void)
{
        // Creates a real temp file and returns its path.
        // sqlite3_open will use it.
        gchar *tmpl = g_strdup ("/tmp/nostrum-storage-test-XXXXXX.db");
        int fd = g_mkstemp (tmpl);
        g_assert_cmpint (fd, >=, 0);
        g_close (fd, NULL);
        return tmpl;
}


// =============================================================================
// HEX CONSTANTS
// =============================================================================

// 64-hex pubkeys (32 bytes)
#define PK_A "f4a1c2d3e4b5968778695a4b3c2d1e0ffedcba98765432100123456789abcdef"
#define PK_B "0b8a6f3c2d1e4f5a6b7c8d9e0f1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c"
#define PK_C "9f4c2a7d8e1b3c5a6d0e2f9a7b8c1d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c"

// 64-hex values for tags 'e' and 'p'
#define TAG_E1 "e4b0c6a2f70a9b1d3e5f60718293a4b5c6d7e8f90123456789abcdeffedcba98"
#define TAG_P1 "c0ffee1234567890deadbeef00112233445566778899aabbccddeeff0011aa22"

// =============================================================================
// HELPERS
// =============================================================================

// Produces a deterministic "fake" signature with correct length (128 hex),
// derived from the computed id (64 hex). Not cryptographically valid, only
// for testing purposes.
static gchar *
fake_sig_from_id (const gchar *id64_hex)
{
        g_return_val_if_fail (id64_hex != NULL, NULL);

        if (strlen (id64_hex) != 64) {
                return g_strdup (
                        "0000000000000000000000000000000000000000000000000000000000000000"
                        "0000000000000000000000000000000000000000000000000000000000000000");
        }

        gchar rev[65];
        for (guint i = 0; i < 64; i++)
                rev[i] = id64_hex[63 - i];
        rev[64] = '\0';

        return g_strconcat (id64_hex, rev, NULL); // 64 + 64 = 128
}

static void
finalize_event (NostrumEvent *e)
{
        g_autoptr (GError) err = NULL;

        nostrum_event_compute_id (e, &err);
        g_assert_no_error (err);

        const gchar *id = nostrum_event_get_id (e);
        g_assert_nonnull (id);
        g_assert_cmpuint (strlen (id), ==, 64);

        // Set a realistic-looking signature
        // (sig is not validated by storage_save)
        g_autofree gchar *sig = fake_sig_from_id (id);
        g_assert_nonnull (sig);
        g_assert_cmpuint (strlen (sig), ==, 128);

        nostrum_event_set_sig (e, sig);
}

// =============================================================================
// FUNCTIONS TO CREATE EVENTS
// =============================================================================

static NostrumEvent *
make_event_1 (void)
{
        // pubkey A, kind 1, created_at 1000, tags: t=bitcoin, e=<64hex>
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_pubkey (e, PK_A);
        nostrum_event_set_created_at (e, 1000L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "hello bitcoin");

        nostrum_event_add_tag (e, "t", "bitcoin", NULL);
        nostrum_event_add_tag (e, "e", TAG_E1, NULL);

        finalize_event (e);
        return e;
}

static NostrumEvent *
make_event_2 (void)
{
        // pubkey A, kind 1, created_at 2000, tags: t=cash, t=bitcoin
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_pubkey (e, PK_A);
        nostrum_event_set_created_at (e, 2000L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "hello cash");

        nostrum_event_add_tag (e, "t", "cash", NULL);
        nostrum_event_add_tag (e, "t", "bitcoin", NULL);

        finalize_event (e);
        return e;
}

static NostrumEvent *
make_event_3 (void)
{
        // pubkey B, kind 7, created_at 1500, tags: p=<64hex>, t=misc
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_pubkey (e, PK_B);
        nostrum_event_set_created_at (e, 1500L);
        nostrum_event_set_kind (e, 7);
        nostrum_event_set_content (e, "hello from B");

        nostrum_event_add_tag (e, "p", TAG_P1, NULL);
        nostrum_event_add_tag (e, "t", "misc", NULL);

        finalize_event (e);
        return e;
}

// =============================================================================
// FUNCTIONS TO CREATE FILTERS
// =============================================================================

static NostrumFilter *
make_filter_authors_kinds_A_kind1 (void)
{
        // authors = [pubkey A], kinds=[1]
        NostrumFilter *f = nostrum_filter_new ();

        const char *authors[] = { PK_A, NULL };
        const int kinds[] = { 1 };

        nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (authors));
        nostrum_filter_take_kinds (f, nostrum_utils_new_int_garray (kinds, 1));

        return f;
}

static NostrumFilter *
make_filter_authors_PK_C (void)
{
        // authors = [pubkey C
        NostrumFilter *f = nostrum_filter_new ();

        const char *authors[] = { PK_C, NULL };

        nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (authors));

        return f;
}

static NostrumFilter *
make_filter_tag_t_cash (void)
{
        // tags: {"#t": ["cash"]}
        NostrumFilter *f = nostrum_filter_new ();

        const char *vals[] = { "cash", NULL };
        GHashTable *ht = nostrum_utils_new_table_str_to_ptrarray ("#t", vals);
        nostrum_filter_take_tags (f, ht);

        return f;
}

static NostrumFilter *
make_filter_author_B (void)
{
        NostrumFilter *f = nostrum_filter_new ();

        const char *authors[] = { PK_B, NULL };
        nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (authors));

        return f;
}

static NostrumFilter *
make_filter_since_1600 (void)
{
        NostrumFilter *f = nostrum_filter_new ();
        nostrum_filter_set_since (f, 1600L);
        return f;
}

static NostrumFilter *
make_filter_author_A_limit_1 (void)
{
        NostrumFilter *f = nostrum_filter_new ();

        const char *authors[] = { PK_A, NULL };
        nostrum_filter_take_authors (f, nostrum_utils_new_str_ptrarray (authors));
        nostrum_filter_set_limit (f, 1);

        return f;
}

// =============================================================================
// SETUP AND TEARDOWN
// =============================================================================

static void
storage_fixture_setup_empty (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        // Create storage ---------------------------------------------------------
        fx->db_path = make_tmp_db_path ();
        fx->storage = nostrum_storage_new (fx->db_path);
        g_assert_nonnull (fx->storage);

        // Init storage -----------------------------------------------------------
        g_autoptr (GError) err = NULL;
        gboolean storage_init_ok = nostrum_storage_init (fx->storage, &err);
        g_assert_true (storage_init_ok);
        g_assert_no_error (err);
}

static void
storage_fixture_setup (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;
        storage_fixture_setup_empty (fx, user_data);
        // Add (save) events ---------------------------------------------------
        g_autoptr (GError) save_err = NULL;

        g_autoptr (NostrumEvent) e1 = make_event_1 ();
        g_assert_true (nostrum_storage_save (fx->storage, e1, &save_err));
        g_assert_no_error (save_err);

        g_clear_error (&save_err);

        g_autoptr (NostrumEvent) e2 = make_event_2 ();
        g_assert_true (nostrum_storage_save (fx->storage, e2, &save_err));
        g_assert_no_error (save_err);

        g_clear_error (&save_err);

        g_autoptr (NostrumEvent) e3 = make_event_3 ();
        g_assert_true (nostrum_storage_save (fx->storage, e3, &save_err));
        g_assert_no_error (save_err);
}


static void
storage_fixture_teardown (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        if (fx->storage) {
                nostrum_storage_free (fx->storage);
                fx->storage = NULL;
        }

        if (fx->db_path) {
                //g_remove (fx->db_path);
                printf ("Removing temp db file: %s\n", fx->db_path);
                g_free (fx->db_path);
                fx->db_path = NULL;
        }
}

// =============================================================================
// TEST CASES
// =============================================================================

static void
test_storage_init_fail (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        // Create storage with invalid path ---------------------------------------
        g_autoptr (GError) err = NULL;
        NostrumStorage *bad_repo = nostrum_storage_new ("/invalid/path/to/db/file.db");
        g_assert_nonnull (bad_repo);

        // Init storage should fail -----------------------------------------------
        gboolean storage_init_ok = nostrum_storage_init (bad_repo, &err);
        g_assert_false (storage_init_ok);
        g_assert_error (err, NOSTRUM_STORAGE_ERROR, NOSTRUM_STORAGE_ERROR_INIT);

        nostrum_storage_free (bad_repo);
}


static void
test_storage_search_returns_no_results (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (NostrumFilter) f = make_filter_authors_PK_C ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f));

        GError *err = NULL;
        g_autoptr(GPtrArray) events = nostrum_storage_search (fx->storage,
                                                           filters,
                                                           &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        g_assert_cmpuint (events->len, ==, 0);

        g_ptr_array_unref(filters);
}

static void
test_storage_search_by_author_and_kind_sorted (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (NostrumFilter) f = make_filter_authors_kinds_A_kind1 ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f));

        GError *err = NULL;
        GPtrArray *events = nostrum_storage_search (fx->storage, filters, &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        // Should match EV2 then EV1 (created_at DESC)
        g_assert_cmpuint (events->len, ==, 2);

        NostrumEvent *e0 = g_ptr_array_index (events, 0);
        NostrumEvent *e1 = g_ptr_array_index (events, 1);
        g_assert_nonnull (e0);
        g_assert_nonnull (e1);

        // Order
        g_assert_cmpint (nostrum_event_get_created_at (e0), ==, 2000L);
        g_assert_cmpint (nostrum_event_get_created_at (e1), ==, 1000L);

        // Both are author A, kind 1
        g_assert_cmpstr (nostrum_event_get_pubkey (e0), ==, PK_A);
        g_assert_cmpint (nostrum_event_get_kind (e0), ==, 1);
        g_assert_cmpstr (nostrum_event_get_pubkey (e1), ==, PK_A);
        g_assert_cmpint (nostrum_event_get_kind (e1), ==, 1);

        // ids look real (computed)
        g_assert_cmpuint (strlen (nostrum_event_get_id (e0)), ==, 64);
        g_assert_cmpuint (strlen (nostrum_event_get_id (e1)), ==, 64);

        g_ptr_array_free (events, TRUE);
        g_ptr_array_free (filters, TRUE);
}

static void
test_storage_search_by_tag_t_cash (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (NostrumFilter) f = make_filter_tag_t_cash ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f));

        GError *err = NULL;
        GPtrArray *events = nostrum_storage_search (fx->storage, filters, &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        // Only EV2 has t=cash (created_at=2000, author A, kind 1)
        g_assert_cmpuint (events->len, ==, 1);

        NostrumEvent *e0 = g_ptr_array_index (events, 0);
        g_assert_nonnull (e0);

        g_assert_cmpint (nostrum_event_get_created_at (e0), ==, 2000L);
        g_assert_cmpstr (nostrum_event_get_pubkey (e0), ==, PK_A);
        g_assert_cmpint (nostrum_event_get_kind (e0), ==, 1);

        g_ptr_array_free (events, TRUE);
        g_ptr_array_free (filters, TRUE);
}

static void
test_storage_search_multiple_filters_or (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        // OR:
        //   filter1: author B -> EV3
        //   filter2: t=cash   -> EV2
        // Result should be EV2(2000), EV3(1500) in DESC order
        g_autoptr (NostrumFilter) f1 = make_filter_author_B ();
        g_autoptr (NostrumFilter) f2 = make_filter_tag_t_cash ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f1));
        g_ptr_array_add (filters, g_steal_pointer (&f2));

        GError *err = NULL;
        GPtrArray *events = nostrum_storage_search (fx->storage, filters, &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        g_assert_cmpuint (events->len, ==, 2);

        NostrumEvent *e0 = g_ptr_array_index (events, 0);
        NostrumEvent *e1 = g_ptr_array_index (events, 1);
        g_assert_nonnull (e0);
        g_assert_nonnull (e1);

        // DESC order by created_at: EV2(2000) then EV3(1500)
        g_assert_cmpint (nostrum_event_get_created_at (e0), ==, 2000L);
        g_assert_cmpint (nostrum_event_get_created_at (e1), ==, 1500L);

        // EV2: author A, kind 1
        g_assert_cmpstr (nostrum_event_get_pubkey (e0), ==, PK_A);
        g_assert_cmpint (nostrum_event_get_kind (e0), ==, 1);

        // EV3: author B, kind 7
        g_assert_cmpstr (nostrum_event_get_pubkey (e1), ==, PK_B);
        g_assert_cmpint (nostrum_event_get_kind (e1), ==, 7);

        g_ptr_array_free (events, TRUE);
        g_ptr_array_free (filters, TRUE);
}

static void
test_storage_search_since (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (NostrumFilter) f = make_filter_since_1600 ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f));

        GError *err = NULL;
        GPtrArray *events = nostrum_storage_search (fx->storage, filters, &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        // since=1600 matches EV2 (2000) only
        g_assert_cmpuint (events->len, ==, 1);

        NostrumEvent *e0 = g_ptr_array_index (events, 0);
        g_assert_nonnull (e0);

        g_assert_cmpint (nostrum_event_get_created_at (e0), ==, 2000L);
        g_assert_cmpstr (nostrum_event_get_pubkey (e0), ==, PK_A);

        g_ptr_array_free (events, TRUE);
        g_ptr_array_free (filters, TRUE);
}

static void
test_storage_search_limit (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        // author A limit=1 -> EV2 only (since ordering is DESC by created_at)
        g_autoptr (NostrumFilter) f = make_filter_author_A_limit_1 ();

        GPtrArray *filters =
            g_ptr_array_new_with_free_func((GDestroyNotify)nostrum_filter_free);
        g_ptr_array_add (filters, g_steal_pointer (&f));

        GError *err = NULL;
        GPtrArray *events = nostrum_storage_search (fx->storage, filters, &err);
        g_assert_nonnull (events);
        g_assert_no_error (err);

        g_assert_cmpuint (events->len, ==, 1);

        NostrumEvent *e0 = g_ptr_array_index (events, 0);
        g_assert_nonnull (e0);

        g_assert_cmpint (nostrum_event_get_created_at (e0), ==, 2000L);
        g_assert_cmpstr (nostrum_event_get_pubkey (e0), ==, PK_A);

        g_ptr_array_free (events, TRUE);
        g_ptr_array_free (filters, TRUE);
}

static void
test_storage_save_duplicate_event (StorageFixture *fx, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (GError) err = NULL;

        // Create a new event --------------------------------------------------
        g_autoptr (NostrumEvent) e = make_event_1 ();

        // First insert should succeed -----------------------------------------
        gboolean ok1 = nostrum_storage_save (fx->storage, e, &err);
        g_assert_true (ok1);
        g_assert_no_error (err);

        // Second insert (same event_id) must fail -----------------------------
        g_clear_error (&err);
        gboolean ok2 = nostrum_storage_save (fx->storage, e, &err);

        g_assert_false (ok2);
        g_assert_error (err,
                        NOSTRUM_STORAGE_ERROR,
                        NOSTRUM_STORAGE_ERROR_DUPLICATE);

        g_test_message ("Error message: %s", err->message ? err->message
                                                          : "(no message)");
        g_assert_nonnull (err->message);
}

// =============================================================================

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add ("/storage/storage_init_fail",
                    StorageFixture,
                    NULL,
                    NULL,
                    test_storage_init_fail,
                    NULL);

        g_test_add ("/storage/search_returns_no_results",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_returns_no_results,
                    storage_fixture_teardown);

        g_test_add ("/storage/search_by_author_and_kind_sorted",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_by_author_and_kind_sorted,
                    storage_fixture_teardown);

        g_test_add ("/storage/search_by_tag_t_cash",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_by_tag_t_cash,
                    storage_fixture_teardown);

        g_test_add ("/storage/search_multiple_filters_or",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_multiple_filters_or,
                    storage_fixture_teardown);

        g_test_add ("/storage/search_since",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_since,
                    storage_fixture_teardown);

        g_test_add ("/storage/search_limit",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup,
                    test_storage_search_limit,
                    storage_fixture_teardown);

        g_test_add ("/storage/save_duplicate_event",
                    StorageFixture,
                    NULL,
                    storage_fixture_setup_empty,
                    test_storage_save_duplicate_event,
                    storage_fixture_teardown);
        
        return g_test_run ();
}
