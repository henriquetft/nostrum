/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgOk                                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-ok.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>

static void
test_ok_fromjson_ok (void)
{
        const char *SAMPLE_JSON =
                "["
                "  \"OK\","
                "  \"a740c637fb027ebe062bdbe188f0b5c9d1d45d0f3b8c0c9b8e0f0f0f0f0f0f\","
                "  true,"
                "  \"saved\""
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (ok);

        g_assert_cmpstr (nostrum_msg_ok_get_id (ok), ==,
                         "a740c637fb027ebe062bdbe188f0b5c9d1d45d0f3b8c0c9b8e0f0f0f0f0f0f");
        g_assert_true (nostrum_msg_ok_get_accepted (ok));
        g_assert_cmpstr (nostrum_msg_ok_get_message (ok), ==, "saved");

        nostrum_msg_ok_free (ok);
        g_clear_error (&err);
}

static void
test_ok_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"OK\", \"id\", true, \"msg\"";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_MSG_OK_ERROR,
                                        NOSTRUM_MSG_OK_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing ok JSON") != NULL);

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "OK message must be a JSON array");

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_len_lt4 (void)
{
        // ["OK", "id", true] -> Missing message
        static const char *BAD_JSON =
                "["
                "  \"OK\","
                "  \"id\","
                "  true"
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "OK message must have at least 4 elements");

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"id\","
                "  true,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of OK message must be a string");

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_type_not_OK (void)
{
        // first element is a string, but not "OK"
        static const char *BAD_JSON =
                "["
                "  \"EVENT\","
                "  \"id\","
                "  true,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of OK message must be \"OK\"");

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_id_not_string (void)
{
        // second element is not a string (event id invalid)
        static const char *BAD_JSON =
                "["
                "  \"OK\","
                "  123,"
                "  true,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of OK message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_accepted_not_boolean (void)
{
        // third element is not boolean
        static const char *BAD_JSON =
                "["
                "  \"OK\","
                "  \"id\","
                "  123,"
                "  \"msg\""
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "Third element of OK message must be a boolean");

        g_clear_error (&err);
}

static void
test_ok_fromjson_error_msg_not_string (void)
{
        // fourth element is not a string
        static const char *BAD_JSON =
                "["
                "  \"OK\","
                "  \"id\","
                "  true,"
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgOk *ok = nostrum_msg_ok_from_json (BAD_JSON, &err);

        g_assert_null (ok);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_OK_ERROR,
                        NOSTRUM_MSG_OK_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Fourth element of OK message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_ok_set_null_ok (void)
{
        NostrumMsgOk *ok = nostrum_msg_ok_new ();

        nostrum_msg_ok_set_id (ok, NULL);
        g_assert_null (nostrum_msg_ok_get_id (ok));

        nostrum_msg_ok_set_message (ok, NULL);
        g_assert_null (nostrum_msg_ok_get_message (ok));

        // default is false
        g_assert_false (nostrum_msg_ok_get_accepted (ok));

        nostrum_msg_ok_free (ok);
}

static void
test_ok_tojson_ok (void)
{
        NostrumMsgOk *ok = nostrum_msg_ok_new ();

        nostrum_msg_ok_set_id (ok, "id-123");
        nostrum_msg_ok_set_accepted (ok, TRUE);
        nostrum_msg_ok_set_message (ok, "saved");

        g_autofree gchar *json = nostrum_msg_ok_to_json (ok);
        g_assert_nonnull (json);

        // parse back to validate structure
        GError *err = NULL;
        NostrumMsgOk *parsed = nostrum_msg_ok_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (parsed);

        g_assert_cmpstr (nostrum_msg_ok_get_id (parsed), ==, "id-123");
        g_assert_true (nostrum_msg_ok_get_accepted (parsed));
        g_assert_cmpstr (nostrum_msg_ok_get_message (parsed), ==, "saved");

        nostrum_msg_ok_free (parsed);
        nostrum_msg_ok_free (ok);
        g_clear_error (&err);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/ok/from_json_ok",                        test_ok_fromjson_ok);
        g_test_add_func ("/ok/from_json_error_parsing_json",        test_ok_fromjson_error_parsing_json);
        g_test_add_func ("/ok/from_json_error_root_not_array",      test_ok_fromjson_error_root_not_array);
        g_test_add_func ("/ok/from_json_error_len_lt4",             test_ok_fromjson_error_len_lt4);
        g_test_add_func ("/ok/from_json_error_type_not_string",     test_ok_fromjson_error_type_not_string);
        g_test_add_func ("/ok/from_json_error_type_not_OK",         test_ok_fromjson_error_type_not_OK);
        g_test_add_func ("/ok/from_json_error_id_not_string",       test_ok_fromjson_error_id_not_string);
        g_test_add_func ("/ok/from_json_error_accepted_not_boolean",
                         test_ok_fromjson_error_accepted_not_boolean);
        g_test_add_func ("/ok/from_json_error_msg_not_string",      test_ok_fromjson_error_msg_not_string);

        g_test_add_func ("/ok/set_null_ok",                         test_ok_set_null_ok);
        g_test_add_func ("/ok/to_json_ok",                          test_ok_tojson_ok);

        return g_test_run ();
}
