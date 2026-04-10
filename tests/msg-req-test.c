/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for NostrumMsgReq                                                    */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-msg-req.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>


static void
test_req_fromjson_ok (void)
{
        const char *SAMPLE_JSON =  "["
               "  \"REQ\","
               "  \"sub-123\","
               "  {},"
               "  {}"
               "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (SAMPLE_JSON, &err);

        g_assert_no_error (err);
        g_assert_nonnull (req);

        g_assert_cmpstr (nostrum_msg_req_get_sub_id (req), ==, "sub-123");

        const GPtrArray *filters = nostrum_msg_req_get_filters (req);
        g_assert_nonnull (filters);
        g_assert_cmpint ((int) filters->len, ==, 2);

        nostrum_msg_req_free (req);
        g_clear_error (&err);
}


static void
test_req_fromjson_error_parsing_json (void)
{
        // malformed JSON
        static const char *BAD_JSON = "[\"REQ\", \"sub-123\", {}";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);

        g_assert_nonnull (err);
        g_assert_true (g_error_matches(err,
                                       NOSTRUM_MSG_REQ_ERROR,
                                       NOSTRUM_MSG_REQ_ERROR_PARSE));

        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Error parsing REQ JSON") != NULL);

        g_clear_error (&err);
}

static void
test_req_fromjson_error_root_not_array (void)
{
        // root is a string, not an array
        static const char *BAD_JSON = "\"not-an-array\"";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message, ==, "REQ message must be a JSON array");

        g_clear_error (&err);
}

static void
test_req_fromjson_error_len_lt3 (void)
{
        // ["REQ", "sub-123"] -> Missing filters
        static const char *BAD_JSON =
                "["
                "  \"REQ\","
                "  \"sub-123\""
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "REQ message must have at least 3 elements");

        g_clear_error (&err);
}

static void
test_req_fromjson_error_type_not_string (void)
{
        // First element is not a string
        static const char *BAD_JSON =
                "["
                "  123,"
                "  \"sub-123\","
                "  {}"
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of REQ message must be a string");

        g_clear_error (&err);
}

static void
test_req_fromjson_error_type_not_REQ (void)
{
        static const char *BAD_JSON =
                "["
                "  \"EVENT\","
                "  \"sub-123\","
                "  {}"
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_cmpstr (err->message,
                         ==,
                         "First element of REQ message must be \"REQ\"");

        g_clear_error (&err);
}

static void
test_req_fromjson_error_subid_not_string (void)
{
        static const char *BAD_JSON =
                "["
                "  \"REQ\","
                "  123,"
                "  {}"
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Second element of REQ message must be a string") != NULL);

        g_clear_error (&err);
}

static void
test_req_fromjson_error_filter_not_object (void)
{
        // terceiro elemento não é objeto
        static const char *BAD_JSON =
                "["
                "  \"REQ\","
                "  \"sub-123\","
                "  \"not-an-object\""
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);
        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Filter element 0 of REQ must be a JSON object") != NULL);

        g_clear_error (&err);
}

static void
test_req_fromjson_error_second_filter_not_object (void)
{
        static const char *BAD_JSON =
                "["
                "  \"REQ\","
                "  \"sub-123\","
                "  {},"
                "  123"
                "]";

        GError *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json (BAD_JSON, &err);

        g_assert_null (req);
        g_assert_nonnull (err);

        g_assert_error (err,
                        NOSTRUM_MSG_REQ_ERROR,
                        NOSTRUM_MSG_REQ_ERROR_PARSE);
        g_assert_nonnull (err->message);

        g_assert_true (g_strstr_len (err->message,
                                     -1,
                                     "Filter element 1 of REQ must be a JSON object") != NULL);

        g_clear_error (&err);
}


static void
test_req_set_null_ok (void)
{
        NostrumMsgReq *req = nostrum_msg_req_new ();

        nostrum_msg_req_set_sub_id (req, NULL);
        g_assert_null (nostrum_msg_req_get_sub_id (req));

        nostrum_msg_req_take_filters (req, NULL);
        g_assert_null (nostrum_msg_req_get_filters (req));

        nostrum_msg_req_free (req);
}



int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/req/from_json_ok",                        test_req_fromjson_ok);
        g_test_add_func ("/req/from_json_error_parsing_json",        test_req_fromjson_error_parsing_json);
        g_test_add_func ("/req/from_json_error_root_not_array",      test_req_fromjson_error_root_not_array);
        g_test_add_func ("/req/from_json_error_len_lt3",             test_req_fromjson_error_len_lt3);
        g_test_add_func ("/req/from_json_error_type_not_string",     test_req_fromjson_error_type_not_string);
        g_test_add_func ("/req/from_json_error_type_not_REQ",        test_req_fromjson_error_type_not_REQ);
        g_test_add_func ("/req/from_json_error_subid_not_string",    test_req_fromjson_error_subid_not_string);
        g_test_add_func ("/req/from_json_error_filter_not_object",   test_req_fromjson_error_filter_not_object);
        g_test_add_func ("/req/from_json_error_second_filter_not_object",
                         test_req_fromjson_error_second_filter_not_object);

        g_test_add_func ("/req/set_null_ok",                         test_req_set_null_ok);

        // how to test this?
        // g_test_add_func ("/req/from_json_error_invalid_filter",
        //                  test_req_from_json_error_invalid_filter);

        return g_test_run ();
}
