/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumEvent                                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include <glib.h>
#include <stdio.h>

#include "nostrum-event.h"

static NostrumEvent *
make_event_correct (void)
{
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_kind (e, 1);
        nostrum_event_set_created_at (e, 1671217411L);
        nostrum_event_set_content (e, "This is a reply to another note!");
        nostrum_event_set_pubkey (e, "dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab");
        nostrum_event_set_id (e, "a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302");
        nostrum_event_set_sig (e, "31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49");

        /* tags */
        nostrum_event_add_tag (
            e,
            "e",
            "5c83da77af1dec6d7289834998ad7aafbd9e2191396d75ec3cc27f5a77226f36",
            "wss://nostr.example.com",
            NULL);
        nostrum_event_add_tag (
            e,
            "p",
            "f7234bd4c1394dda46d09f35bd384dd30cc552ad5541990f98844fb06676e9ca",
            NULL);

        return e;
}

static NostrumEvent *
make_event_sigfail (void)
{
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_id (e, "4376c65d2f232afbe9b882a35baa4f6fe8667c4e684749af565f981833ed6a65");
        nostrum_event_set_pubkey (e, "6e468422dfb74a5738702a8823b9b28168abab8655faacb6853cd0ee15deee93");
        nostrum_event_set_created_at (e, 1673347337L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "Hello Nostr! This is my first note.");
        nostrum_event_set_sig (e, "31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49");

        nostrum_event_add_tag (
            e,
            "e",
            "5c83da77af1dec6d7289834998ad7aafbd9e2191396d75ec3cc27f5a77226f36",
            NULL);
        nostrum_event_add_tag (
            e,
            "p",
            "f7234bd4c1394dda46d09f35bd384dd30cc552ad5541990f98844fb06676e9ca",
            NULL);

        return e;
}

static void
test_computeid_ok ()
{
        g_autoptr (GError) err = NULL;
        g_autoptr (NostrumEvent) event = make_event_correct ();
        nostrum_event_compute_id (event, &err);
        g_assert_cmpstr (nostrum_event_get_id (event), ==,
                         "a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302");
}

static void
test_serialize_error_pubkey_null ()
{
        GError *err = NULL;
        g_autoptr (NostrumEvent) event = make_event_correct ();
        nostrum_event_set_pubkey (event, NULL);
        nostrum_event_serialize (event, &err);

        g_assert_error (err, NOSTRUM_EVENT_ERROR, NOSTRUM_EVENT_ERROR_SERIALIZE);
        g_assert_nonnull (err->message);
        g_clear_error (&err);
}


static void
test_verifysig_error_fail ()
{
        g_autoptr (NostrumEvent) event = make_event_sigfail ();
        GError *err = NULL;
        gboolean result = nostrum_event_verify_sig (event, &err);
        g_assert_false (result);

        g_assert_error (err, NOSTRUM_EVENT_ERROR, NOSTRUM_EVENT_ERROR_BAD_SIG);
        g_assert_nonnull (err->message);

        g_clear_error (&err);
}

static void
test_verifysig_ok ()
{
        NostrumEvent *event = make_event_correct ();
        GError *err = NULL;
        gboolean ok = nostrum_event_verify_sig (event, &err);
        g_assert_true (ok);
        g_assert_null (err);
        nostrum_event_free (event);
}


static void
test_event_fromjson_ok (void)
{
        static const char *SAMPLE_JSON =
                "{\n"
                "  \"id\": \"a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302\",\n"
                "  \"pubkey\": \"dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab\",\n"
                "  \"created_at\": 1671217411,\n"
                "  \"kind\": 1,\n"
                "  \"tags\": [\n"
                "    [\"e\", \"5c83da77af1dec6d7289834998ad7aafbd9e2191396d75ec3cc27f5a77226f36\", \"wss://nostr.example.com\"],\n"
                "    [\"p\", \"f7234bd4c1394dda46d09f35bd384dd30cc552ad5541990f98844fb06676e9ca\"]\n"
                "  ],\n"
                "  \"content\": \"This is a reply to another note!\",\n"
                "  \"sig\": \"31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49\"\n"
                "}\n";

        GError *err = NULL;

        g_autoptr (NostrumEvent) event = nostrum_event_from_json (SAMPLE_JSON, &err);
        g_assert_no_error (err);
        g_assert_nonnull (event);

        g_assert_cmpstr (nostrum_event_get_id (event),
                         ==,
                         "a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302");

        g_assert_cmpstr (nostrum_event_get_pubkey (event),
                         ==,
                         "dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab");

        g_assert_cmpint ((int)nostrum_event_get_created_at (event), ==, 1671217411);
        g_assert_cmpint (nostrum_event_get_kind (event), ==, 1);

        g_assert_cmpstr (nostrum_event_get_content (event),
                         ==,
                         "This is a reply to another note!");

        g_assert_cmpstr (nostrum_event_get_sig (event),
                        ==,
                        "31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49");

        const GPtrArray *tags = nostrum_event_get_tags (event);
        g_assert_nonnull (tags);
        g_assert_cmpint ((int) tags->len, ==, 2);


        const GPtrArray *t0 = g_ptr_array_index ((GPtrArray *) tags, 0);
        g_assert_nonnull (t0);
        g_assert_cmpint ((int) t0->len, ==, 3);

        const char *t0_0 = g_ptr_array_index ((GPtrArray *)t0, 0);
        const char *t0_1 = g_ptr_array_index ((GPtrArray *)t0, 1);
        const char *t0_2 = g_ptr_array_index ((GPtrArray *)t0, 2);

        g_assert_cmpstr (t0_0, ==, "e");
        g_assert_cmpstr (t0_1, ==, "5c83da77af1dec6d7289834998ad7aafbd9e2191396d75ec3cc27f5a77226f36");
        g_assert_cmpstr (t0_2, ==, "wss://nostr.example.com");


        const GPtrArray *t1 = g_ptr_array_index ((GPtrArray *)tags, 1);
        g_assert_nonnull (t1);
        g_assert_cmpint ((int) t1->len, ==, 2);

        const char *t1_0 = g_ptr_array_index ((GPtrArray *) t1, 0);
        const char *t1_1 = g_ptr_array_index ((GPtrArray *) t1, 1);

        g_assert_cmpstr (t1_0, ==, "p");
        g_assert_cmpstr (t1_1, ==, "f7234bd4c1394dda46d09f35bd384dd30cc552ad5541990f98844fb06676e9ca");

        g_clear_error (&err);
}


static void
test_from_json_error_non_object_root (void)
{
        static const char *BAD_JSON = "[\"not\", \"an\", \"object\"]";

        GError *err = NULL;
        NostrumEvent *ev = nostrum_event_from_json (BAD_JSON, &err);

        g_assert_null (ev);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_EVENT_ERROR,
                                        NOSTRUM_EVENT_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "Event JSON must be an object");

        g_clear_error (&err);
}

static void
test_fromjson_error_parsing_json (void)
{
        static const char *BAD_JSON = "[\"error\", \"parsing\", \"object\"";

        GError *err = NULL;
        NostrumEvent *ev = nostrum_event_from_json (BAD_JSON, &err);

        g_assert_null (ev);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches(err,
                                       NOSTRUM_EVENT_ERROR,
                                       NOSTRUM_EVENT_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing event JSON") != NULL);

        g_clear_error (&err);
}

static void
test_fromjson_error_tags_not_array (void)
{
        static const char *BAD_JSON =
                "{\n"
                "  \"id\": \"a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302\",\n"
                "  \"pubkey\": \"dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab\",\n"
                "  \"created_at\": 1671217411,\n"
                "  \"kind\": 1,\n"
                "  \"tags\": { },\n"
                "  \"content\": \"This is a reply to another note!\",\n"
                "  \"sig\": \"31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49\"\n"
                "}\n";

        GError *err = NULL;
        NostrumEvent *ev = nostrum_event_from_json (BAD_JSON, &err);

        g_assert_null (ev);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_EVENT_ERROR,
                                        NOSTRUM_EVENT_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "'tags' must be an array") != NULL);

        g_clear_error (&err);
}

static void
test_fromjson_error_tags_not_inner_array (void)
{
        static const char *BAD_JSON =
                "{\n"
                "  \"id\": \"a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302\",\n"
                "  \"pubkey\": \"dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab\",\n"
                "  \"created_at\": 1671217411,\n"
                "  \"kind\": 1,\n"
                "  \"tags\": [ \"a\" ],\n"
                "  \"content\": \"This is a reply to another note!\",\n"
                "  \"sig\": \"31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49\"\n"
                "}\n";

        GError *err = NULL;
        NostrumEvent *ev = nostrum_event_from_json (BAD_JSON, &err);

        g_assert_null (ev);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_EVENT_ERROR,
                                        NOSTRUM_EVENT_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "] must be an array") != NULL);

        g_clear_error (&err);
}

static void
test_fromjson_error_tags_not_string(void)
{
        static const char *BAD_JSON =
                "{\n"
                "  \"id\": \"a2e434c25df7ef9ccc9a559ffd8f635b00b090b327e0a89c5b7284228026b302\",\n"
                "  \"pubkey\": \"dbd591c4fa6a2918679985b83f812e5482d08660764c2d63850566c163e82aab\",\n"
                "  \"created_at\": 1671217411,\n"
                "  \"kind\": 1,\n"
                "  \"tags\": [ [ { } ] ],\n"
                "  \"content\": \"This is a reply to another note!\",\n"
                "  \"sig\": \"31fc6727c475a973edda4e664c2035096da9780409f7b8be084311812a90210b83c1bd04b8f42487c16711b26b7a41b0356d33bd0f8b519c6aeaf8969d7e6b49\"\n"
                "}\n";

        GError *err = NULL;
        NostrumEvent *ev = nostrum_event_from_json (BAD_JSON, &err);

        g_assert_null (ev);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_EVENT_ERROR,
                                        NOSTRUM_EVENT_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "] must be string") != NULL);

        g_clear_error (&err);
}

static void
test_set_null_ok(void)
{
        NostrumEvent * event = make_event_correct ();
        
        nostrum_event_set_id (event, NULL);
        g_assert_null (nostrum_event_get_id (event));

        nostrum_event_set_pubkey (event, NULL);
        g_assert_null (nostrum_event_get_pubkey (event));

        nostrum_event_set_content (event, NULL); 
        g_assert_null (nostrum_event_get_content (event));

        nostrum_event_take_tags (event, NULL);
        g_assert_null (nostrum_event_get_tags (event));

        nostrum_event_free (event);
}

// =============================================================================

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/event/setnull_ok", test_set_null_ok);
        g_test_add_func ("/event/compute_id_ok", test_computeid_ok);
        g_test_add_func ("/event/serialize_error_pubkey_null", test_serialize_error_pubkey_null);
        g_test_add_func ("/event/verify_sig_ok", test_verifysig_ok);
        g_test_add_func ("/event/verify_sig_error_fail", test_verifysig_error_fail);
        g_test_add_func ("/event/from_json_ok", test_event_fromjson_ok);
        g_test_add_func ("/event/from_json_error_non_object_root", test_from_json_error_non_object_root);
        g_test_add_func ("/event/from_json_error_parsing_json", test_fromjson_error_parsing_json);
        g_test_add_func ("/event/from_json_error_tags_not_array", test_fromjson_error_tags_not_array);
        g_test_add_func ("/event/from_json_error_tags_not_inner_array", test_fromjson_error_tags_not_inner_array);
        g_test_add_func ("/event/from_json_error_tags_not_string", test_fromjson_error_tags_not_string);
        

        return g_test_run ();
}

