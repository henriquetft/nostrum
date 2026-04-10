/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgClosed                                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-closed.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>

static void
test_closed_fromjson_ok (void)
{
        const char *SAMPLE_JSON =
                "["
                "  \"CLOSED\","
                "  \"sub-123\","
                "  \"error: invalid subscription\""
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (closed);

        g_assert_cmpstr (nostrum_msg_closed_get_subscription_id (closed), ==, "sub-123");
        g_assert_cmpstr (nostrum_msg_closed_get_message (closed), ==, "error: invalid subscription");

        nostrum_msg_closed_free (closed);
        g_clear_error (&err);
}

static void
test_closed_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"CLOSED\", \"sub-123\", \"msg\"";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_MSG_CLOSED_ERROR,
                                        NOSTRUM_MSG_CLOSED_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing CLOSED JSON") != NULL);

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "CLOSED message must be a JSON array");

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_len_lt3 (void)
{
        // ["CLOSED", "sub-123"] -> Missing message
        static const char *BAD_JSON =
                "["
                "  \"CLOSED\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "CLOSED message must have at least 3 elements");

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"sub-123\","
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of CLOSED message must be a string");

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_type_not_CLOSED (void)
{
        // first element is a string, but not "CLOSED"
        static const char *BAD_JSON =
                "["
                "  \"EOSE\","
                "  \"sub-123\","
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of CLOSED message must be \"CLOSED\"");

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_subid_not_string (void)
{
        // second element is not string (subscription id invalid)
        static const char *BAD_JSON =
                "["
                "  \"CLOSED\","
                "  123,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of CLOSED message must "
                                     "be a string")
                        != NULL);

        g_clear_error (&err);
}

static void
test_closed_fromjson_error_msg_not_string (void)
{
        // third element is not string (message invalid)
        static const char *BAD_JSON =
                "["
                "  \"CLOSED\","
                "  \"sub-123\","
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgClosed *closed = nostrum_msg_closed_from_json (BAD_JSON, &err);

        g_assert_null (closed);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_CLOSED_ERROR,
                        NOSTRUM_MSG_CLOSED_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Third element of CLOSED message must "
                                     "be a string")
                       != NULL);

        g_clear_error (&err);
}

static void
test_closed_set_null_ok (void)
{
        NostrumMsgClosed *closed = nostrum_msg_closed_new ();

        nostrum_msg_closed_set_subscription_id (closed, NULL);
        g_assert_null (nostrum_msg_closed_get_subscription_id (closed));

        nostrum_msg_closed_set_message (closed, NULL);
        g_assert_null (nostrum_msg_closed_get_message (closed));

        nostrum_msg_closed_free (closed);
}

static void
test_closed_tojson_ok (void)
{
        NostrumMsgClosed *closed = nostrum_msg_closed_new ();

        nostrum_msg_closed_set_subscription_id (closed, "sub-123");
        nostrum_msg_closed_set_message (closed, "error: invalid subscription");

        g_autofree gchar *json = nostrum_msg_closed_to_json (closed);
        g_assert_nonnull (json);

        // parse back to validate structure
        GError *err = NULL;
        NostrumMsgClosed *parsed = nostrum_msg_closed_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (parsed);

        g_assert_cmpstr (nostrum_msg_closed_get_subscription_id (parsed), ==, "sub-123");
        g_assert_cmpstr (nostrum_msg_closed_get_message (parsed), ==, "error: invalid subscription");

        nostrum_msg_closed_free (parsed);
        nostrum_msg_closed_free (closed);
        g_clear_error (&err);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/closed/from_json_ok",                        test_closed_fromjson_ok);
        g_test_add_func ("/closed/from_json_error_parsing_json",        test_closed_fromjson_error_parsing_json);
        g_test_add_func ("/closed/from_json_error_root_not_array",      test_closed_fromjson_error_root_not_array);
        g_test_add_func ("/closed/from_json_error_len_lt3",             test_closed_fromjson_error_len_lt3);
        g_test_add_func ("/closed/from_json_error_type_not_string",     test_closed_fromjson_error_type_not_string);
        g_test_add_func ("/closed/from_json_error_type_not_CLOSED",     test_closed_fromjson_error_type_not_CLOSED);
        g_test_add_func ("/closed/from_json_error_subid_not_string",    test_closed_fromjson_error_subid_not_string);
        g_test_add_func ("/closed/from_json_error_msg_not_string",      test_closed_fromjson_error_msg_not_string);

        g_test_add_func ("/closed/set_null_ok",                         test_closed_set_null_ok);
        g_test_add_func ("/closed/to_json_ok",                          test_closed_tojson_ok);

        return g_test_run ();
}
