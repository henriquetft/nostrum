/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgClose                                                  */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-close.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>


static void
test_close_fromjson_ok (void)
{
        const char *SAMPLE_JSON =
                "["
                "  \"CLOSE\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (close);

        g_assert_cmpstr (nostrum_msg_close_get_subscription_id (close), ==, "sub-123");

        nostrum_msg_close_free (close);
        g_clear_error (&err);
}

static void
test_close_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"CLOSE\", \"sub-123\"";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_MSG_CLOSE_ERROR,
                                        NOSTRUM_MSG_CLOSE_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing CLOSE JSON") != NULL);

        g_clear_error (&err);
}

static void
test_close_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSE_ERROR,
                        NOSTRUM_MSG_CLOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "CLOSE message must be a JSON array");

        g_clear_error (&err);
}

static void
test_close_fromjson_error_len_lt2 (void)
{
        // ["CLOSE"] -> Missing subscription id
        static const char *BAD_JSON =
                "["
                "  \"CLOSE\""
                "]";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSE_ERROR,
                        NOSTRUM_MSG_CLOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "CLOSE message must have at least 2 elements");

        g_clear_error (&err);
}

static void
test_close_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSE_ERROR,
                        NOSTRUM_MSG_CLOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of CLOSE message must be a string");

        g_clear_error (&err);
}

static void
test_close_fromjson_error_type_not_CLOSE (void)
{
        // First element is a string, but not "CLOSE"
        static const char *BAD_JSON =
                "["
                "  \"REQ\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSE_ERROR,
                        NOSTRUM_MSG_CLOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of CLOSE message must be \"CLOSE\"");

        g_clear_error (&err);
}

static void
test_close_fromjson_error_subid_not_string (void)
{
        // second element is not string (subscription id invalid)
        static const char *BAD_JSON =
                "["
                "  \"CLOSE\","
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgClose *close = nostrum_msg_close_from_json (BAD_JSON, &err);

        g_assert_null (close);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSE_ERROR,
                        NOSTRUM_MSG_CLOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of CLOSE message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_close_set_null_ok (void)
{
        NostrumMsgClose *close = nostrum_msg_close_new ();
        nostrum_msg_close_set_subscription_id (close, "sub-123");
        nostrum_msg_close_set_subscription_id (close, NULL);
        g_assert_null (nostrum_msg_close_get_subscription_id (close));

        nostrum_msg_close_free (close);
}

static void
test_close_tojson_ok (void)
{
        NostrumMsgClose *close = nostrum_msg_close_new ();

        nostrum_msg_close_set_subscription_id (close, "sub-123");

        g_autofree gchar *json = nostrum_msg_close_to_json (close);
        g_assert_nonnull (json);

        // Parse back to validate structure
        GError *err = NULL;
        NostrumMsgClose *parsed = nostrum_msg_close_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (parsed);

        g_assert_cmpstr (nostrum_msg_close_get_subscription_id (parsed), ==, "sub-123");

        nostrum_msg_close_free (parsed);
        nostrum_msg_close_free (close);
        g_clear_error (&err);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/close/from_json_ok",                        test_close_fromjson_ok);
        g_test_add_func ("/close/from_json_error_parsing_json",        test_close_fromjson_error_parsing_json);
        g_test_add_func ("/close/from_json_error_root_not_array",      test_close_fromjson_error_root_not_array);
        g_test_add_func ("/close/from_json_error_len_lt2",             test_close_fromjson_error_len_lt2);
        g_test_add_func ("/close/from_json_error_type_not_string",     test_close_fromjson_error_type_not_string);
        g_test_add_func ("/close/from_json_error_type_not_CLOSE",      test_close_fromjson_error_type_not_CLOSE);
        g_test_add_func ("/close/from_json_error_subid_not_string",    test_close_fromjson_error_subid_not_string);

        g_test_add_func ("/close/set_null_ok",                         test_close_set_null_ok);
        g_test_add_func ("/close/to_json_ok",                          test_close_tojson_ok);

        return g_test_run ();
}
