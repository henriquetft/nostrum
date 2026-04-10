/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgNotice                                                 */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-notice.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>

static void
test_notice_fromjson_ok (void)
{
        const char *SAMPLE_JSON =
                "["
                "  \"NOTICE\","
                "  \"The quick brown fox jumps over the lazy dogs\""
                "]";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (notice);

        g_assert_cmpstr (nostrum_msg_notice_get_message (notice), ==,
                         "The quick brown fox jumps over the lazy dogs");

        nostrum_msg_notice_free (notice);
        g_clear_error (&err);
}


static void
test_notice_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"NOTICE\", \"msg\"";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_MSG_NOTICE_ERROR,
                                        NOSTRUM_MSG_NOTICE_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing NOTICE JSON") != NULL);

        g_clear_error (&err);
}

static void
test_notice_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_NOTICE_ERROR,
                        NOSTRUM_MSG_NOTICE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "NOTICE message must be a JSON array");

        g_clear_error (&err);
}

static void
test_notice_fromjson_error_len_lt2 (void)
{
        // ["NOTICE"] -> Missing message
        static const char *BAD_JSON =
                "["
                "  \"NOTICE\""
                "]";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_NOTICE_ERROR,
                        NOSTRUM_MSG_NOTICE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "NOTICE message must have at least 2 elements");

        g_clear_error (&err);
}

static void
test_notice_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_NOTICE_ERROR,
                        NOSTRUM_MSG_NOTICE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of NOTICE message must be a string");

        g_clear_error (&err);
}

static void
test_notice_fromjson_error_type_not_NOTICE (void)
{
        // first element is a string, but not "NOTICE"
        static const char *BAD_JSON =
                "["
                "  \"EOSE\","
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_NOTICE_ERROR,
                        NOSTRUM_MSG_NOTICE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of NOTICE message must be \"NOTICE\"");

        g_clear_error (&err);
}

static void
test_notice_fromjson_error_msg_not_string (void)
{
        // second element is not string (message invalid)
        static const char *BAD_JSON =
                "["
                "  \"NOTICE\","
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgNotice *notice = nostrum_msg_notice_from_json (BAD_JSON, &err);

        g_assert_null (notice);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_NOTICE_ERROR,
                        NOSTRUM_MSG_NOTICE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of NOTICE message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_notice_set_null_ok (void)
{
        NostrumMsgNotice *notice = nostrum_msg_notice_new ();

        nostrum_msg_notice_set_message (notice, NULL);
        g_assert_null (nostrum_msg_notice_get_message (notice));

        nostrum_msg_notice_free (notice);
}

static void
test_notice_tojson_ok (void)
{
        NostrumMsgNotice *notice = nostrum_msg_notice_new ();

        nostrum_msg_notice_set_message (notice, "Relay will restart in 5 minutes");

        g_autofree gchar *json = nostrum_msg_notice_to_json (notice);
        g_assert_nonnull (json);

        // Parse back to validate structure
        GError *err = NULL;
        NostrumMsgNotice *parsed = nostrum_msg_notice_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (parsed);

        g_assert_cmpstr (nostrum_msg_notice_get_message (parsed), ==,
                         "Relay will restart in 5 minutes");

        nostrum_msg_notice_free (parsed);
        nostrum_msg_notice_free (notice);
        g_clear_error (&err);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/notice/from_json_ok",                        test_notice_fromjson_ok);
        g_test_add_func ("/notice/from_json_error_parsing_json",        test_notice_fromjson_error_parsing_json);
        g_test_add_func ("/notice/from_json_error_root_not_array",      test_notice_fromjson_error_root_not_array);
        g_test_add_func ("/notice/from_json_error_len_lt2",             test_notice_fromjson_error_len_lt2);
        g_test_add_func ("/notice/from_json_error_type_not_string",     test_notice_fromjson_error_type_not_string);
        g_test_add_func ("/notice/from_json_error_type_not_NOTICE",     test_notice_fromjson_error_type_not_NOTICE);
        g_test_add_func ("/notice/from_json_error_msg_not_string",      test_notice_fromjson_error_msg_not_string);

        g_test_add_func ("/notice/set_null_ok",                         test_notice_set_null_ok);
        g_test_add_func ("/notice/to_json_ok",                          test_notice_tojson_ok);

        return g_test_run ();
}
