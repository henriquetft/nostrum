/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgEose                                                   */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-eose.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>

static void
test_eose_fromjson_ok (void)
{
        const char *SAMPLE_JSON =
                "["
                "  \"EOSE\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (eose);

        g_assert_cmpstr (nostrum_msg_eose_get_subscription_id (eose), ==, "sub-123");

        nostrum_msg_eose_free (eose);
        g_clear_error (&err);
}


static void
test_eose_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"EOSE\", \"sub-123\"";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches (err,
                                        NOSTRUM_MSG_EOSE_ERROR,
                                        NOSTRUM_MSG_EOSE_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing EOSE JSON") != NULL);

        g_clear_error (&err);
}

static void
test_eose_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_EOSE_ERROR,
                        NOSTRUM_MSG_EOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "EOSE message must be a JSON array");

        g_clear_error (&err);
}

static void
test_eose_fromjson_error_len_lt2 (void)
{
        // ["EOSE"] -> Missing subscription id
        static const char *BAD_JSON =
                "["
                "  \"EOSE\""
                "]";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_EOSE_ERROR,
                        NOSTRUM_MSG_EOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "EOSE message must have at least 2 elements");

        g_clear_error (&err);
}

static void
test_eose_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_EOSE_ERROR,
                        NOSTRUM_MSG_EOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of EOSE message must be a string");

        g_clear_error (&err);
}

static void
test_eose_fromjson_error_type_not_EOSE (void)
{
        // first element is a string, but not "EOSE"
        static const char *BAD_JSON =
                "["
                "  \"CLOSE\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_EOSE_ERROR,
                        NOSTRUM_MSG_EOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of EOSE message must be \"EOSE\"");

        g_clear_error (&err);
}

static void
test_eose_fromjson_error_subid_not_string (void)
{
        // second element is not string (subscription id invalid)
        static const char *BAD_JSON =
                "["
                "  \"EOSE\","
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgEose *eose = nostrum_msg_eose_from_json (BAD_JSON, &err);

        g_assert_null (eose);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_EOSE_ERROR,
                        NOSTRUM_MSG_EOSE_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of EOSE message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_eose_set_null_ok (void)
{
        NostrumMsgEose *eose = nostrum_msg_eose_new ();

        nostrum_msg_eose_set_subscription_id (eose, NULL);
        g_assert_null (nostrum_msg_eose_get_subscription_id (eose));

        nostrum_msg_eose_free (eose);
}

static void
test_eose_tojson_ok (void)
{
        NostrumMsgEose *eose = nostrum_msg_eose_new ();

        nostrum_msg_eose_set_subscription_id (eose, "sub-123");

        g_autofree gchar *json = nostrum_msg_eose_to_json (eose);
        g_assert_nonnull (json);

        // parse back to validate structure
        GError *err = NULL;
        NostrumMsgEose *parsed = nostrum_msg_eose_from_json (json, &err);

        g_assert_no_error (err);
        g_assert_nonnull (parsed);

        g_assert_cmpstr (nostrum_msg_eose_get_subscription_id (parsed), ==, "sub-123");

        nostrum_msg_eose_free (parsed);
        nostrum_msg_eose_free (eose);
        g_clear_error (&err);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/eose/from_json_ok",                        test_eose_fromjson_ok);
        g_test_add_func ("/eose/from_json_error_parsing_json",        test_eose_fromjson_error_parsing_json);
        g_test_add_func ("/eose/from_json_error_root_not_array",      test_eose_fromjson_error_root_not_array);
        g_test_add_func ("/eose/from_json_error_len_lt2",             test_eose_fromjson_error_len_lt2);
        g_test_add_func ("/eose/from_json_error_type_not_string",     test_eose_fromjson_error_type_not_string);
        g_test_add_func ("/eose/from_json_error_type_not_EOSE",       test_eose_fromjson_error_type_not_EOSE);
        g_test_add_func ("/eose/from_json_error_subid_not_string",    test_eose_fromjson_error_subid_not_string);

        g_test_add_func ("/eose/set_null_ok",                         test_eose_set_null_ok);
        g_test_add_func ("/eose/to_json_ok",                          test_eose_tojson_ok);

        return g_test_run ();
}
