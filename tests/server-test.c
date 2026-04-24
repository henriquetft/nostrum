/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Tests for relay                                                            */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-relay.h"
#include "nostrum-config.h"
#include "nostrum-msg-ok.h"
#include "nostrum-event.h"
#include "nostrum-msg-req.h"
#include "nostrum-filter.h"
#include "nostrum-msg-closed.h"
#include "nostrum-msg-close.h"
#include "nostrum-utils.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>


#define NOSTRUM_TEST_PORT       18080
#define NOSTRUM_TEST_HOSTNAME   "127.0.0.1"

typedef struct _MyFixture       MyFixture;
typedef struct _TestCaseConfig  TestCaseConfig;
typedef struct _ConnectCtx      ConnectCtx;


typedef void (*ConnectCallback) (SoupWebsocketConnection  *conn,
                                 MyFixture                *fixture);

static NostrumEvent *make_event_correct (void);
static void          start_test         (MyFixture *f, gconstpointer user_data);
static void          generic_on_connect (GObject       *source,
                                         GAsyncResult  *res,
                                         gpointer       conn_context);

struct _MyFixture
{

        const TestCaseConfig *cfg;

        NostrumRelay *relay;        // owned
        GMainLoop    *loop;         // owned
        SoupSession  *session;      // owned
        GPtrArray    *clients;      // owned, SoupWebsocketConnection* 
        gboolean      test_done;
        gboolean      test_success;
        guint         timeout_id;
        gchar        *tmpdir;       // owned
        gchar        *db_path;      // owned


        // Variables for the subscription-forward test
        SoupWebsocketConnection *sub_conn; // subscriber client
        SoupWebsocketConnection *pub_conn; // publisher client
        gboolean sub_connected;   // Subscriber connected
        gboolean pub_connected;   // Publisher connected
        gboolean flow_started;    // Start of testcase. After sending REQ+EVENT
        gboolean event_received;  // If subscriber received the event

        // Variables for other tests
        GList *events_received;  // List of NostrumEvent* received by client
        gboolean duplicate_test_event_ok;
};


struct _TestCaseConfig
{
        const char       *name;         // Test name
        ConnectCallback  *on_connects;  // Callbacks of client connections
        gsize             n_clients;    // Number of clients
};

// Pass parameterer to generic_on_connect
struct _ConnectCtx
{
        ConnectCallback   callback;
        MyFixture        *fixture;
};

static void
generic_on_connect (GObject                 *source,
                    GAsyncResult            *res,
                    gpointer                 conn_context);
static void
on_ws_closed       (SoupWebsocketConnection *c,
                    gpointer                 user_data);


// Secret keys 32 bytes (64-hex)
#define SK_A "0000000000000000000000000000000000000000000000000000000000000001"
#define SK_B "0000000000000000000000000000000000000000000000000000000000000002"
#define SK_C "0000000000000000000000000000000000000000000000000000000000000003"

// 64-hex pubkeys (32 bytes)
#define PK_A "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
#define PK_B "c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5"
#define PK_C "f9308a019258c31049344f85f89d5229b531c845836f99b08601f113bce036f9"

// 64-hex values for tags 'e' and 'p'
#define TAG_E1 "e4b0c6a2f70a9b1d3e5f60718293a4b5c6d7e8f90123456789abcdeffedcba98"
#define TAG_P1 "c0ffee1234567890deadbeef00112233445566778899aabbccddeeff0011aa22"


static gboolean
hex_to_bytes_32 (const gchar *hex64, guint8 out32[32])
{
        g_return_val_if_fail (hex64 != NULL, FALSE);
        if (strlen (hex64) != 64)
                return FALSE;

        for (guint i = 0; i < 32; i++) {
                gint hi = g_ascii_xdigit_value (hex64[i * 2]);
                gint lo = g_ascii_xdigit_value (hex64[i * 2 + 1]);
                if (hi < 0 || lo < 0)
                        return FALSE;
                out32[i] = (guint8)((hi << 4) | lo);
        }

        return TRUE;
}

static gchar *
bytes_to_hex (const guint8 *bytes, gsize len)
{
        static const gchar hexmap[] = "0123456789abcdef";
        gchar *s = g_malloc0 (len * 2 + 1);

        for (gsize i = 0; i < len; i++) {
                s[i * 2]     = hexmap[(bytes[i] >> 4) & 0xF];
                s[i * 2 + 1] = hexmap[bytes[i] & 0xF];
        }

        return s;
}

static gchar *
derive_pubkey_from_seckey_hex (const gchar *seckey_hex, GError **err)
{
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        guint8 sk[32];
        if (!hex_to_bytes_32 (seckey_hex, sk)) {
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "seckey must be 64-hex (32 bytes)");
                return NULL;
        }

        secp256k1_context *ctx =
                secp256k1_context_create (SECP256K1_CONTEXT_SIGN);

        if (!secp256k1_ec_seckey_verify (ctx, sk)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "invalid secp256k1 secret key");
                return NULL;
        }

        secp256k1_keypair kp;
        if (!secp256k1_keypair_create (ctx, &kp, sk)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "secp256k1_keypair_create failed");
                return NULL;
        }

        secp256k1_xonly_pubkey xpub;
        int pk_parity = 0;
        if (!secp256k1_keypair_xonly_pub (ctx, &xpub, &pk_parity, &kp)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "secp256k1_keypair_xonly_pub failed");
                return NULL;
        }

        guint8 pub32[32];
        if (!secp256k1_xonly_pubkey_serialize (ctx, pub32, &xpub)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "secp256k1_xonly_pubkey_serialize failed");
                return NULL;
        }

        secp256k1_context_destroy (ctx);
        return bytes_to_hex (pub32, 32);
}


 // This is just for testing. Do not use in production.
static gchar *
sign_id (const gchar   *seckey_hex,
         const gchar   *id_hex,
         GError       **err)
{
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        guint8 sk[32];
        guint8 msg32[32];

        if (!hex_to_bytes_32 (seckey_hex, sk)) {
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "seckey must be 64-hex (32 bytes)");
                return NULL;
        }

        if (!hex_to_bytes_32 (id_hex, msg32)) {
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "id must be 64-hex (32 bytes)");
                return NULL;
        }

        secp256k1_context *ctx =
                secp256k1_context_create (SECP256K1_CONTEXT_SIGN);

        if (!secp256k1_ec_seckey_verify (ctx, sk)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "invalid secp256k1 secret key");
                return NULL;
        }

        secp256k1_keypair kp;
        if (!secp256k1_keypair_create (ctx, &kp, sk)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "secp256k1_keypair_create failed");
                return NULL;
        }

        guint8 aux_rand[32] = {0};

        guint8 sig64[64];
        if (!secp256k1_schnorrsig_sign32 (ctx, sig64, msg32, &kp, aux_rand)) {
                secp256k1_context_destroy (ctx);
                g_set_error (err, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "secp256k1_schnorrsig_sign32 failed");
                return NULL;
        }

        secp256k1_context_destroy (ctx);
        return bytes_to_hex (sig64, 64);
}

static void
finalize_event_with_keys (NostrumEvent *e,
                          const gchar  *pubkey_hex,
                          const gchar  *seckey_hex)
{
        g_autoptr (GError) err = NULL;

        g_return_if_fail (e != NULL);
        g_return_if_fail (pubkey_hex != NULL);
        g_return_if_fail (seckey_hex != NULL);

        g_assert_cmpuint (strlen (pubkey_hex), ==, 64);
        g_assert_cmpuint (strlen (seckey_hex), ==, 64);

        nostrum_event_set_pubkey (e, pubkey_hex);

        // compute id (NIP-01)
        nostrum_event_compute_id (e, &err);
        g_assert_no_error (err);

        const gchar *id = nostrum_event_get_id (e);
        g_assert_nonnull (id);
        g_assert_cmpuint (strlen (id), ==, 64);

        // Signing
        g_autofree gchar *sig = sign_id (seckey_hex, id, &err);
        g_assert_no_error (err);
        g_assert_nonnull (sig);
        g_assert_cmpuint (strlen (sig), ==, 128);

        nostrum_event_set_sig (e, sig);
}


static void
print_test_pubkeys (void)
{
        g_autoptr (GError) err = NULL;

        g_autofree gchar *a = derive_pubkey_from_seckey_hex (SK_A, &err);
        g_assert_no_error (err);
        g_autofree gchar *b = derive_pubkey_from_seckey_hex (SK_B, &err);
        g_assert_no_error (err);
        g_autofree gchar *c = derive_pubkey_from_seckey_hex (SK_C, &err);
        g_assert_no_error (err);

        g_print ("#define PK_A \"%s\"\n", a);
        g_print ("#define PK_B \"%s\"\n", b);
        g_print ("#define PK_C \"%s\"\n", c);
}

// =============================================================================
// GETTERS
// =============================================================================

static NostrumEvent *
make_event_1 (void)
{
        // author A, kind 1, created_at 1000, tags: t=bitcoin, e=<64hex>
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_created_at (e, 1000L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "hello bitcoin");

        nostrum_event_add_tag (e, "t", "bitcoin", NULL);
        nostrum_event_add_tag (e, "e", TAG_E1, NULL);

        finalize_event_with_keys (e, PK_A, SK_A);
        return e;
}

static NostrumEvent *
make_event_2 (void)
{
        // author A, kind 1, created_at 2000, tags: t=cash, t=bitcoin
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_created_at (e, 2000L);
        nostrum_event_set_kind (e, 1);
        nostrum_event_set_content (e, "hello cash");

        nostrum_event_add_tag (e, "t", "cash", NULL);
        nostrum_event_add_tag (e, "t", "bitcoin", NULL);

        finalize_event_with_keys (e, PK_A, SK_A);
        return e;
}

static NostrumEvent *
make_event_3 (void)
{
        // author B, kind 7, created_at 1500, tags: p=<64hex>, t=misc
        NostrumEvent *e = nostrum_event_new ();

        nostrum_event_set_created_at (e, 1500L);
        nostrum_event_set_kind (e, 7);
        nostrum_event_set_content (e, "hello from B");

        nostrum_event_add_tag (e, "p", TAG_P1, NULL);
        nostrum_event_add_tag (e, "t", "misc", NULL);

        finalize_event_with_keys (e, PK_B, SK_B);
        return e;
}


// =============================================================================


static void
connect_client (MyFixture        *f,
                ConnectCallback   callback)
{
        ConnectCtx *conn_ctx = g_new0 (ConnectCtx, 1);
        conn_ctx->callback = callback;
        conn_ctx->fixture = f;

        g_autoptr (SoupMessage) msg = NULL;

        gchar *uri = g_strdup_printf ("ws://%s:%u/ws",
                                      NOSTRUM_TEST_HOSTNAME,
                                      NOSTRUM_TEST_PORT);
        msg = soup_message_new ("GET", uri);
        g_free (uri);

        soup_session_websocket_connect_async (f->session,
                                              msg,
                                              NULL,
                                              NULL,
                                              G_PRIORITY_DEFAULT,
                                              NULL,
                                              generic_on_connect,
                                              conn_ctx);
}

static void
ws_fixture_setup (MyFixture *f, gconstpointer user_data)
{
        (void)user_data;

        g_autoptr (GError) error = NULL;

        f->loop = g_main_loop_new (NULL, FALSE);
        f->timeout_id = 0;

        f->tmpdir = g_dir_make_tmp ("nostrum-relay-test-XXXXXX", &error);
        g_assert_no_error (error);
        g_assert_nonnull (f->tmpdir);

        f->db_path = g_build_filename (f->tmpdir, "nostrum_relay.db", NULL);
        g_assert_nonnull (f->db_path);

        g_message ("File db for test: %s", f->db_path);

        struct NostrumRelayConfig cfg;
        nostrum_relay_config_init (&cfg);
        cfg.server_http_port = NOSTRUM_TEST_PORT;
        cfg.server_host = NOSTRUM_TEST_HOSTNAME;
        cfg.db_path = f->db_path;
        cfg.db_type = "sqlite";
        cfg.info_name = "Test Relay";
        cfg.info_description = "Relay for testing";
        cfg.info_contact = "admin@test.com";

        f->relay = nostrum_relay_new (&cfg);
        g_assert_nonnull (f->relay);

        gboolean ok = nostrum_relay_listen (f->relay, &error);
        g_assert_true (ok);
        g_assert_no_error (error);

        f->session = soup_session_new ();

        f->clients =
            g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

        f->test_done = FALSE;
        f->test_success = FALSE;
        f->duplicate_test_event_ok = FALSE;

        f->events_received = NULL;
}

static void
ws_fixture_teardown (MyFixture *f, gconstpointer user_data)
{
        (void)user_data;

        if (f->timeout_id != 0) {
                g_source_remove (f->timeout_id);
                f->timeout_id = 0;
        }

        if (f->clients) {
                for (guint i = 0; i < f->clients->len; i++) {
                        SoupWebsocketConnection *c =
                          g_ptr_array_index (f->clients, i);
                        if (!c)
                                continue;

                        soup_websocket_connection_close (c,
                                                SOUP_WEBSOCKET_CLOSE_NORMAL,
                                                "test teardown");
                }

                g_ptr_array_free (f->clients, TRUE);
                f->clients = NULL;
        }

        if (f->session) {
                g_object_unref (f->session);
                f->session = NULL;
        }

        if (f->relay) {
                nostrum_relay_free (f->relay);
                f->relay = NULL;
        }

        if (f->loop) {
                g_main_loop_unref (f->loop);
                f->loop = NULL;
        }

        if (f->events_received) {
                g_list_free_full (f->events_received,
                                  (GDestroyNotify)nostrum_event_free);
                f->events_received = NULL;
        }

        if (f->db_path) {
                g_remove (f->db_path);
                g_clear_pointer (&f->db_path, g_free);
        }

        if (f->tmpdir) {
                g_rmdir (f->tmpdir);
                g_clear_pointer (&f->tmpdir, g_free);
        }
}


static gboolean
test_timeout_cb (gpointer user_data)
{
        MyFixture *f = user_data;

        g_assert_nonnull (f);
        g_assert_nonnull (f->loop);
        g_test_message ("Test timed out: %s", f->cfg->name);

        if (!f->test_done) {
                f->test_success = FALSE;
                f->test_done = TRUE;
        }

        g_test_message ("Quitting main loop %p", f->loop);
        g_main_loop_quit (f->loop);

        f->timeout_id = 0;
        return G_SOURCE_REMOVE;
}



// =============================================================================
//  TEST: simple client connection
// =============================================================================

static void
test_connect_on_connect (SoupWebsocketConnection  *conn,
                         MyFixture                *fixture)
{
        g_test_message ("Client connected sucessfully!");

        fixture->test_success = TRUE;
        fixture->test_done = TRUE;
        g_main_loop_quit (fixture->loop);
}


static const TestCaseConfig cfg_connect = {
        .name = "test_connect",
        .on_connects = (ConnectCallback[]) {
                test_connect_on_connect,
        },

        .n_clients = 1
};


// =============================================================================
//  TEST: bad signature
// =============================================================================

static void
test_badsig_on_message (SoupWebsocketConnection *conn,
                        gint                    type,
                        GBytes                  *message,
                        gpointer                fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("testbadsig received: type=%d, payload='%.*s'",
                        type, (int)size, data);

        GError *err = NULL;
        g_autoptr (NostrumMsgOk) ok = nostrum_msg_ok_from_json(data, &err);
        if (err) {
                g_test_message ("testbadsig received invalid OK: %s",
                                err->message);
                g_clear_error(&err);
        } else {
                f->test_success = nostrum_msg_ok_get_accepted(ok) == FALSE &&
                                  g_strstr_len(nostrum_msg_ok_get_message(ok),
                                               -1,
                                               "bad signature");
                f->test_done = TRUE;
                g_main_loop_quit (f->loop);
        }
}

static void
test_badsig_on_connect (SoupWebsocketConnection  *conn,
                        MyFixture                *fixture)
{
        g_signal_connect (conn, "message",
                          G_CALLBACK (test_badsig_on_message), fixture);

        const gchar *event_msg = "[\"EVENT\",{"
                                 "\"id\":\"test-id\","
                                 "\"pubkey\":\"test-pubkey\","
                                 "\"created_at\":1234567890,"
                                 "\"kind\":1,"
                                 "\"tags\":[],"
                                 "\"content\":\"hello from publisher\","
                                 "\"sig\":\"test-sig\""
                                 "}]";

        g_test_message ("Sending event with bad signature: %s", event_msg);
        soup_websocket_connection_send_text (conn, event_msg);


}

static const TestCaseConfig cfg_badsig = {
        .name = "test_badsig",
        .on_connects = (ConnectCallback[]) {
                test_badsig_on_connect,
        },

        .n_clients = 1
};


// =============================================================================
//  TEST: duplicate event
// =============================================================================

static void
test_duplicate_on_message (SoupWebsocketConnection *conn,
                           gint                    type,
                           GBytes                  *message,
                           gpointer                fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        g_test_message ("testduplicate received: type=%d, payload='%.*s'",
                        type, (int)size, data);

        GError *err = NULL;
        g_autoptr (NostrumMsgOk) ok = nostrum_msg_ok_from_json(data, &err);
        if (err) {
                g_test_message ("testduplicate received invalid OK: %s",
                                err->message);
                g_clear_error(&err);
        } else {
                
                if (nostrum_msg_ok_get_accepted(ok) == TRUE && !g_strcmp0(nostrum_msg_ok_get_message(ok), "")) {
                                                g_test_message(" entrou aqui!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (%s)", nostrum_msg_ok_get_message(ok));
                                                f->duplicate_test_event_ok = TRUE;
                                               }
                else if (nostrum_msg_ok_get_accepted(ok) == TRUE && f->duplicate_test_event_ok &&
                                  !g_strcmp0(nostrum_msg_ok_get_message(ok),
                                               "duplicate: already have this event")) {
                       f->test_done = TRUE;
                       f->test_success = TRUE;
                       g_main_loop_quit (f->loop);
                       
                }                               

        }
}

static void
test_duplicate_on_connect (SoupWebsocketConnection  *conn,
                           MyFixture                *fixture)
{
        g_signal_connect (conn, "message",
                          G_CALLBACK (test_duplicate_on_message), fixture);

        g_autoptr(NostrumEvent) event = make_event_1 ();
        g_autofree gchar *event_json = nostrum_event_to_json (event);
        g_autofree gchar *event_msg = g_strdup_printf ("[\"EVENT\",%s]", event_json);
        
        g_test_message ("Sending event 1: %s", event_msg);
        soup_websocket_connection_send_text (conn, event_msg);

        g_test_message ("Sending event 2 (duplicate): %s", event_msg);
        soup_websocket_connection_send_text (conn, event_msg);


}

static const TestCaseConfig cfg_duplicate = {
        .name = "test_duplicate",
        .on_connects = (ConnectCallback[]) {
                test_duplicate_on_connect,
        },

        .n_clients = 1
};

// =============================================================================
//  TEST: forward event (subscrption)
// =============================================================================


static void
test_subsforward_on_sub_message (SoupWebsocketConnection *conn,
                                 gint                     type,
                                 GBytes                  *message,
                                 gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Subscriber received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        // FIXME build event
        // ["EVENT","sub1", ...]
        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
            g_str_has_prefix (data, "[\"EVENT\"") &&
            g_strstr_len (data, size, "\"sub1\"") != NULL) {
                f->event_received = TRUE;
                f->test_success = TRUE;
                f->test_done = TRUE;
                g_main_loop_quit (f->loop);
        }
}


static void
test_subsforward_on_pub_message (SoupWebsocketConnection *conn,
                                 gint                     type,
                                 GBytes                  *message,
                                 gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Publisher received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        g_autoptr(NostrumMsgOk) ok = nostrum_msg_ok_from_json(data, NULL);
        if (ok) {
                if (nostrum_msg_ok_get_accepted(ok)) {
                        g_test_message("publisher received OK accepted for the sent event");
                } else {
                        g_test_message("publisher received rejected OK for the sent event: %s",
                                       nostrum_msg_ok_get_message(ok));
                        g_main_loop_quit (f->loop);
                }
        }
}

static void
test_subsforward_req_first_maybe_start_flow (MyFixture *fixture)
{
        if (fixture->flow_started)
                return;

        if (!fixture->sub_connected || !fixture->pub_connected)
                return;

        fixture->flow_started = TRUE;

        g_debug (" Starting %s flow .......", fixture->cfg->name);
        // Send REQ from subscriber
         NostrumFilter *filter = nostrum_filter_new ();
        nostrum_filter_take_kinds (filter,
                                   nostrum_utils_new_int_garray ((gint[]){1}, 1));

        NostrumMsgReq *req = nostrum_msg_req_new ();
        nostrum_msg_req_set_sub_id (req, "sub1");
        nostrum_msg_req_take_one_filter (req, filter);
        g_autofree gchar *req_json = nostrum_msg_req_to_json (req);
        g_test_message ("Subscriber sending REQ: %s", req_json);
        soup_websocket_connection_send_text (fixture->sub_conn, req_json);

        // Send EVENT from publisher
        g_autoptr(NostrumEvent) event = make_event_correct();
        g_autofree gchar *event_json = nostrum_event_to_json(event);
        g_autofree gchar *event_msg = g_strdup_printf("[\"EVENT\",%s]", event_json);
        g_test_message ("Publisher sending event: %s", event_msg);
        soup_websocket_connection_send_text (fixture->pub_conn, event_msg);
        nostrum_msg_req_free (req);
}


static void
test_subsforward_req_first_on_connect_sub (SoupWebsocketConnection *conn,
                                           MyFixture               *fixture)
{
        fixture->sub_conn      = conn;
        fixture->sub_connected = TRUE;

        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_subsforward_on_sub_message),
                          fixture);

        g_test_message ("Subscriber connected!");

        test_subsforward_req_first_maybe_start_flow (fixture);
}


static void
test_subsforward_req_first_on_connect_pub (SoupWebsocketConnection *conn,
                                           MyFixture   *fixture)
{
        fixture->pub_conn      = conn;
        fixture->pub_connected = TRUE;
        
        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_subsforward_on_pub_message),
                          fixture);

        g_test_message ("Publisher connected!");

        test_subsforward_req_first_maybe_start_flow (fixture);
}


static const TestCaseConfig cfg_subsforward_req_first = {
        .name = "test_subsforward_req_first",
        .on_connects = (ConnectCallback[]) {
                test_subsforward_req_first_on_connect_sub,
                test_subsforward_req_first_on_connect_pub,
        },

        .n_clients = 2
};

// =============================================================================

static void
test_reqinvalidfilter_on_message (SoupWebsocketConnection *conn,
                                  gint                     type,
                                  GBytes                  *message,
                                  gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Subscriber received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10
            && g_str_has_prefix (data, "[\"CLOSED\"")
            && g_strstr_len (data, size, "\"sub-123\"") != NULL) {
                f->event_received = TRUE;
                f->test_success = TRUE;
                f->test_done = TRUE;
                g_main_loop_quit (f->loop);
        }
}

static void
test_reqinvalidfilter_on_connect (SoupWebsocketConnection *conn,
                                  MyFixture               *fixture)
{
        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_reqinvalidfilter_on_message),
                          fixture);

        const char *SAMPLE_JSON =  "["
               "  \"REQ\","
               "  \"sub-123\","
               "  [],"
               "  {}"
               "]";

        g_test_message ("Sending bad REQ: %s", SAMPLE_JSON);
        soup_websocket_connection_send_text (conn, SAMPLE_JSON);
}

static const TestCaseConfig cfg_reqinvalidfilter = {
        .name = "test_reqinvalidfilter",
        .on_connects = (ConnectCallback[]) {
                test_reqinvalidfilter_on_connect,
        },

        .n_clients = 1
};

// =============================================================================

static void
test_unkonwnfilterelement_on_message (SoupWebsocketConnection *conn,
                                      gint                     type,
                                      GBytes                  *message,
                                      gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        GError *err = NULL;
        g_autoptr (NostrumMsgClosed) ok = nostrum_msg_closed_from_json(data, &err);
        if (err) {
                g_test_message ("unkonwnfilterelement error: %s", err->message);
                g_clear_error(&err);
        } else {
                f->test_success = g_strstr_len(nostrum_msg_closed_get_message(ok),
                                               -1,
                                               "unsupported: filter contains unknown elements")
                                  != NULL;
        }

        f->test_done = TRUE;
        g_main_loop_quit (f->loop);
}

static void
test_unkonwnfilterelement_on_connect (SoupWebsocketConnection *conn,
                                      MyFixture               *fixture)
{
        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_unkonwnfilterelement_on_message),
                          fixture);

        static const char *SAMPLE_JSON =
                "["
                "  \"REQ\","
                "  \"sub-123\","
                "  {"
                "    \"kinds\": [1],"
                "    \"authors\": [\"abcdef123456\"],"
                "    \"this_field_does_not_exist\": 21"
                "  }"
                "]";
        g_test_message ("Sending bad REQ: %s", SAMPLE_JSON);
        soup_websocket_connection_send_text (conn, SAMPLE_JSON);
}

static const TestCaseConfig cfg_unkonwnfilterelement = {
        .name = "test_unkonwnfilterelement",
        .on_connects = (ConnectCallback[]) {
                test_unkonwnfilterelement_on_connect,
        },

        .n_clients = 1
};

// =============================================================================
// TEST: request stored events
// =============================================================================

static void
test_reqstoredevents_start_flow (MyFixture *fixture)
{
        if (fixture->flow_started)
                return;

        if (!fixture->sub_connected || !fixture->pub_connected)
                return;

        fixture->flow_started = TRUE;

        g_debug (" Starting %s flow .......", fixture->cfg->name);

        // PUBLISHER: Send event kind=1
        {
                g_autoptr(NostrumEvent) event = make_event_1 ();
                g_autofree gchar *event_json = nostrum_event_to_json (event);
                g_autofree gchar *event_msg = g_strdup_printf ("[\"EVENT\",%s]", event_json);
                g_test_message ("Publisher sending event: %s", event_msg);
                soup_websocket_connection_send_text (fixture->pub_conn, event_msg);
        }

        // PUBLISHER: Send event kind=1
        {
                g_autoptr(NostrumEvent) event = make_event_2 ();
                g_autofree gchar *event_json = nostrum_event_to_json (event);
                g_autofree gchar *event_msg = g_strdup_printf ("[\"EVENT\",%s]", event_json);
                g_test_message ("Publisher sending event: %s", event_msg);
                soup_websocket_connection_send_text (fixture->pub_conn, event_msg);
        }

        // PUBLISHER: Send event kind=7
        {
                g_autoptr (NostrumEvent) event = make_event_3();
                g_autofree gchar *event_json = nostrum_event_to_json(event);
                g_autofree gchar *event_msg = g_strdup_printf("[\"EVENT\",%s]", event_json);
                g_test_message ("Publisher sending event: %s", event_msg);
                soup_websocket_connection_send_text (fixture->pub_conn, event_msg);
        }

        // SUBSCRIBER: Subscribe for events kind=1
        {
                g_autoptr(NostrumFilter) filter = nostrum_filter_new ();
                nostrum_filter_take_kinds (filter,
                                       nostrum_utils_new_int_garray ((gint[]){1}, 1));

                g_autoptr (NostrumMsgReq) req = nostrum_msg_req_new ();
                nostrum_msg_req_set_sub_id (req, "sub-999");
                nostrum_msg_req_take_one_filter (req, g_steal_pointer (&filter));
                g_autofree gchar *req_json = nostrum_msg_req_to_json (req);
                g_test_message ("Subscriber sending REQ: %s", req_json);
                soup_websocket_connection_send_text (fixture->sub_conn, req_json);
        }
}

static NostrumEvent *
parse_event_msg_extract_event (const gchar *data, gsize size)
{
        g_return_val_if_fail (data != NULL, NULL);

        g_autoptr(JsonParser) p = json_parser_new ();
        if (!json_parser_load_from_data (p, data, (gssize)size, NULL))
                return NULL;

        JsonNode *root = json_parser_get_root (p);
        if (!JSON_NODE_HOLDS_ARRAY (root))
                return NULL;

        JsonArray *arr = json_node_get_array (root);
        if (json_array_get_length (arr) < 3)
                return NULL;

        const char *type = json_array_get_string_element (arr, 0);
        if (!type || g_strcmp0 (type, "EVENT") != 0)
                return NULL;


        JsonNode *ev_node = json_array_get_element (arr, 2);
        if (!ev_node || !JSON_NODE_HOLDS_OBJECT (ev_node))
                return NULL;

        g_autoptr(JsonGenerator) gen = json_generator_new ();
        json_generator_set_root (gen, ev_node);
        g_autofree char *ev_json = json_generator_to_data (gen, NULL);

        if (!ev_json)
                return NULL;

        return nostrum_event_from_json (ev_json, NULL);
}

static void
test_reqstoredevents_on_message_pub (SoupWebsocketConnection *conn,
                                     gint                     type,
                                     GBytes                  *message,
                                     gpointer                 fixture)
{
        (void)conn;
        (void)type;
        (void)message;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Publisher received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        f->test_success = FALSE;
        f->test_done = TRUE;
        g_main_loop_quit (f->loop);
}

static void
test_reqstoredevents_on_message_sub (SoupWebsocketConnection *conn,
                                     gint                     type,
                                     GBytes                  *message,
                                     gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Subscriber received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        // FIXME build event
        // ["EVENT","sub1", ...]
        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
            g_str_has_prefix (data, "[\"EVENT\"") &&
            g_strstr_len (data, size, "\"sub-999\"") != NULL) {
                NostrumEvent *ev = parse_event_msg_extract_event (data, size);
                if (ev) {
                        f->events_received = g_list_append (f->events_received, ev);
                } else {
                        g_test_message ("Failed to parse received event: %s", data);
                }

        } else if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
                   g_str_has_prefix (data, "[\"EOSE\"") &&
                   g_strstr_len (data, size, "\"sub-999\"") != NULL) {
                
                f->test_done = TRUE;

                guint n_events = g_list_length (f->events_received);
                g_test_message ("Received %u events", n_events);

                if (n_events != 2) {
                        g_test_message ("Expected 2 events, but received %u", n_events);
                        f->test_success = FALSE;
                } else {
                        g_autoptr (NostrumEvent) event1 = make_event_1 ();
                        g_autoptr (NostrumEvent) event2 = make_event_2 ();
                        // check all messages received
                        // iterate over f->events_received

                        GList *l;
                        gboolean matched_event1 = FALSE;
                        gboolean matched_event2 = FALSE;
                        for (l = f->events_received; l != NULL; l = l->next) {
                                gpointer data = l->data;
                                NostrumEvent *ev = data;

                                if (g_strcmp0 (nostrum_event_get_id (ev), nostrum_event_get_id (event1)) == 0) {
                                        matched_event1 = TRUE;
                                }

                                if (g_strcmp0 (nostrum_event_get_id (ev), nostrum_event_get_id (event2)) == 0) {
                                        matched_event2 = TRUE;
                                }
                        }


                        f->test_success = (matched_event1 && matched_event2);
                }

                g_main_loop_quit (f->loop);
        }
}


static void
test_reqstoredevents_on_connect_sub (SoupWebsocketConnection *conn,
                                     MyFixture               *fixture)
{
        fixture->sub_conn      = conn;
        fixture->sub_connected = TRUE;

        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_reqstoredevents_on_message_sub),
                          fixture);

        g_message ("Subscriber connected");
        test_reqstoredevents_start_flow (fixture);
}

static void
test_reqstoredevents_on_connect_pub (SoupWebsocketConnection *conn,
                                     MyFixture               *fixture)
{
        fixture->pub_conn      = conn;
        fixture->pub_connected = TRUE;
        
        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_reqstoredevents_on_message_pub),
                          fixture);
                
        g_message ("Publisher connected");
        test_reqstoredevents_start_flow (fixture);
}

static const TestCaseConfig cfg_reqstoredevents = {
        .name = "test_reqstoredevents",
        .on_connects = (ConnectCallback[]) {
                test_reqstoredevents_on_connect_pub,
                test_reqstoredevents_on_connect_sub,
        },

        .n_clients = 2
};


// =============================================================================
// TEST: unsubscribe
// =============================================================================

static void
test_unsubscribe_on_message_pub (SoupWebsocketConnection *conn,
                                 gint                     type,
                                 GBytes                  *message,
                                 gpointer                 fixture)
{
        (void)conn;
        (void)type;
        (void)message;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Publisher received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
            g_str_has_prefix (data, "[\"EVENT\"")) {
                g_test_message ("Publisher should not receive any events");
                // Fail the test immediately
                MyFixture *f = fixture;
                f->test_success = FALSE;
                f->test_done = TRUE;
                g_main_loop_quit (f->loop);
        }
}

static gboolean
publish_event_later (gpointer user_data)
{
        MyFixture *f = user_data;
        g_autoptr (NostrumEvent) event = make_event_2 ();
        g_autofree gchar *event_json = nostrum_event_to_json (event);
        g_autofree gchar *event_msg =
        g_strdup_printf ("[\"EVENT\",%s]", event_json);

        g_test_message ("Publisher sending event (idle): %s", event_msg);

        soup_websocket_connection_send_text (f->pub_conn, event_msg);

        return G_SOURCE_REMOVE;
}

static void
test_unsubscribe_on_message_sub (SoupWebsocketConnection *conn,
                                 gint                     type,
                                 GBytes                  *message,
                                 gpointer                 fixture)
{
        (void)conn;
        MyFixture *f = fixture;

        gsize size = 0;
        const gchar *data = g_bytes_get_data (message, &size);

        g_test_message ("(%s) Subscriber received: type=%d, payload='%.*s'",
                        f->cfg->name, type, (int)size, data);

        // FIXME build event
        // ["EVENT","sub1", ...]
        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
            g_str_has_prefix (data, "[\"EVENT\"") &&
            g_strstr_len (data, size, "\"sub-777\"") != NULL) {
                NostrumEvent *ev = parse_event_msg_extract_event (data, size);
                if (ev) {
                        f->events_received = g_list_append (f->events_received, ev);
                        f->test_success = f->test_done = g_list_length (f->events_received) == 1;
                        g_test_message ("After Received event test_sucess=%d, test_done=%d count_events=%d",
                                         f->test_success, f->test_done, g_list_length (f->events_received));
                        
                } else {
                        g_test_message ("Failed to parse received event: %s", data);
                }
        }

        if (type == SOUP_WEBSOCKET_DATA_TEXT && size >= 10 &&
                g_str_has_prefix (data, "[\"EOSE\"") &&
                g_strstr_len (data, size, "\"sub-777\"") != NULL) {
                // Unsubscribe right after receiving the event
                {
                        g_autoptr (NostrumMsgClose) close = nostrum_msg_close_new ();
                        nostrum_msg_close_set_subscription_id (close, "sub-777");
                        g_autofree gchar *close_json = nostrum_msg_close_to_json (close);
                        g_test_message ("Subscriber sending CLOSE: %s", close_json);
                        soup_websocket_connection_send_text (f->sub_conn, close_json);
                }

                // PUBLISHER: Send event kind=1 (should not be received)
                // (only after loop iteration)
                // Ensure the CLOSE has been processed by the event loop
                g_idle_add (publish_event_later, f);
        }
}

static void
test_unsubscribe_start_flow (MyFixture *fixture)
{
        if (fixture->flow_started)
                return;

        if (!fixture->sub_connected || !fixture->pub_connected)
                return;

        fixture->flow_started = TRUE;

        g_debug ("Starting %s flow  .......", fixture->cfg->name);

        // SUBSCRIBER: Subscribe for events kind=1
        {
                g_autoptr(NostrumFilter) filter = nostrum_filter_new ();
                nostrum_filter_take_kinds (filter,
                                       nostrum_utils_new_int_garray ((gint[]){1}, 1));

                g_autoptr (NostrumMsgReq) req = nostrum_msg_req_new ();
                nostrum_msg_req_set_sub_id (req, "sub-777");
                nostrum_msg_req_take_one_filter (req, g_steal_pointer (&filter));
                g_autofree gchar *req_json = nostrum_msg_req_to_json (req);
                g_test_message ("Subscriber sending REQ: %s", req_json);
                soup_websocket_connection_send_text (fixture->sub_conn, req_json);
        }

        // PUBLISHER: Send event kind=1
        {
                g_autoptr(NostrumEvent) event = make_event_1 ();
                g_autofree gchar *event_json = nostrum_event_to_json (event);
                g_autofree gchar *event_msg = g_strdup_printf ("[\"EVENT\",%s]", event_json);
                g_test_message ("Publisher sending event: %s", event_msg);
                soup_websocket_connection_send_text (fixture->pub_conn, event_msg);
        }

}

static void
test_unsubscribe_on_connect_sub (SoupWebsocketConnection *conn,
                                 MyFixture               *fixture)
{
        fixture->sub_conn      = conn;
        fixture->sub_connected = TRUE;

        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_unsubscribe_on_message_sub),
                          fixture);

        g_message ("Subscriber connected");
        test_unsubscribe_start_flow (fixture);
}

static void
test_unsubscribe_on_connect_pub (SoupWebsocketConnection *conn,
                                 MyFixture               *fixture)
{
        fixture->pub_conn      = conn;
        fixture->pub_connected = TRUE;
        
        g_signal_connect (conn,
                          "message",
                          G_CALLBACK (test_unsubscribe_on_message_pub),
                          fixture);
                
        g_message ("Publisher connected");
        test_unsubscribe_start_flow (fixture);
}

static const TestCaseConfig cfg_unsubscribe = {
        .name = "test_unsubscribe",
        .on_connects = (ConnectCallback[]) {
                test_unsubscribe_on_connect_pub,
                test_unsubscribe_on_connect_sub,
        },

        .n_clients = 2
};

// =============================================================================

int
main (int argc, char **argv)
{
        
        g_test_init (&argc, &argv, NULL);
        
        g_test_add ("/websocket/test_connect",
                    MyFixture,
                    &cfg_connect,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

        g_test_add ("/websocket/test_send_event_bad_sig",
                    MyFixture,
                    &cfg_badsig,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);
        
        g_test_add ("/websocket/test_send_event_duplicate",
                    MyFixture,
                    &cfg_duplicate,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

        g_test_add ("/websocket/req-invalid-filter",
                    MyFixture,
                    &cfg_reqinvalidfilter,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

        g_test_add ("/websocket/subscription-forward-req-first",
                    MyFixture,
                    &cfg_subsforward_req_first,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

        g_test_add ("/websocket/req-unkonwnfilterelement",
                    MyFixture,
                    &cfg_unkonwnfilterelement,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

        g_test_add ("/websocket/req-stored-events",
                    MyFixture,
                    &cfg_reqstoredevents,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);
        
        g_test_add ("/websocket/unsubscribe",
                    MyFixture,
                    &cfg_unsubscribe,
                    ws_fixture_setup,
                    start_test,
                    ws_fixture_teardown);

         return g_test_run ();
}


// =============================================================================

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

        // Tags
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

static void
start_test (MyFixture *f, gconstpointer user_data)
{
        g_assert_nonnull (user_data);
        const TestCaseConfig *cfg = user_data;

        g_message ("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        g_message ("+++ Running test case: %s", cfg->name);

        f->cfg = cfg;

        for (gsize i = 0; i < cfg->n_clients; i++) {
                ConnectCallback callback = cfg->on_connects[i];
                connect_client (f, callback);
        }

        f->timeout_id = g_timeout_add_seconds (5, test_timeout_cb, f);

        g_main_loop_run (f->loop);

        g_message ("+++ Test case completed: %s", cfg->name);
        g_message ("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        g_assert_true (f->test_success);

        //soup_websocket_connection_close(f->conn)
        // FIXME close connection here
}

static void
generic_on_connect (GObject       *source,
                    GAsyncResult  *res,
                    gpointer       conn_context)
{
        ConnectCtx *conn_ctx = conn_context;

        MyFixture *f = conn_ctx->fixture;
        g_autoptr (GError) error = NULL;

        SoupWebsocketConnection *conn =
            soup_session_websocket_connect_finish (SOUP_SESSION (source),
                                                   res,
                                                   &error);
        g_signal_connect (conn, "closed", G_CALLBACK (on_ws_closed), NULL);

        g_assert_no_error (error);
        g_assert_nonnull (conn);

        // Keep connections to not be deallocated
        g_ptr_array_add (f->clients, g_object_ref (conn));

        // Call ConnectCallback for the test
        conn_ctx->callback (conn, f);

        g_free (conn_ctx);
}

static void
on_ws_closed (SoupWebsocketConnection *c, gpointer user_data) {
        g_message ("  WS closed: %p", c);
}

// testar close de subscription