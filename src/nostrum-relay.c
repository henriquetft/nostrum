/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumRelay object (implementation of nostrum-relay.h)                    */
/* A nostr server                                                             */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-event.h"
#include "nostrum-config.h"
#include "nostrum-msg-req.h"
#include "nostrum-filter.h"
#include "nostrum-msg-closed.h"
#include "nostrum-msg-eose.h"
#include "nostrum-subscription.h"
#include "nostrum-json-utils.h"
#include "nostrum-relay.h"
#include "nostrum-msg-ok.h"
#include "nostrum-utils.h"
#include "nostrum-storage.h"
#include "nostrum-version.h"
#include <gio/gio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>


#define G_LOG_DOMAIN "nostrum-relay"

struct _NostrumRelay
{
        SoupServer                  *server;
        NostrumStorage              *storage;
        struct NostrumRelayConfig   *cfg;
};

// FIXME should be part of NostrumRelay struct
// List of connected clients
static GList *connections_list = NULL;


static
void msg_event_to_subscriber (SoupWebsocketConnection        *conn,
                              const NostrumSubscription      *sub,
                              const NostrumEvent             *event);

static
void msg_response_to_event   (SoupWebsocketConnection        *conn,
                              const NostrumEvent             *event,
                              gboolean                        accepted,
                              const gchar                    *reason);
static
GPtrArray *get_subscriptions_from_connection (SoupWebsocketConnection *conn);

static void
on_nip11        (SoupServer               *server,
                 SoupServerMessage        *msg,
                 const char               *path,
                 GHashTable               *query,
                 gpointer                  user_data);

static void
on_ws_connected (SoupServer               *server,
                 SoupServerMessage        *msg,
                 const char               *path,
                 SoupWebsocketConnection  *conn,
                 gpointer                  user_data);

static gboolean
listen_on_host (SoupServer                *server,
                const char                *host,
                guint                      port,
                SoupServerListenOptions    opts,
                GError                   **error);

// =============================================================================

// curl -H 'Accept: application/nostr+json' http://localhost:8080/

NostrumRelay *
nostrum_relay_new (const struct NostrumRelayConfig *cfg)
{
        NostrumRelay *relay = g_new0 (NostrumRelay, 1);

        // Copying config
        relay->cfg = g_new0(struct NostrumRelayConfig, 1);
        nostrum_relay_config_init(relay->cfg);
        nostrum_relay_config_copy(relay->cfg, cfg);

        relay->server = soup_server_new (NULL);

        // WS em "/"
        soup_server_add_websocket_handler (relay->server,
                                           "/",
                                           NULL,
                                           NULL,
                                           on_ws_connected,
                                           relay,
                                           NULL);

        // NIP-11
        soup_server_add_handler (relay->server, "/", on_nip11, relay->cfg, NULL);

        // Storage
        g_info ("Initializing relay storage with db path: %s",
                relay->cfg->db_path);
        relay->storage = nostrum_storage_new (relay->cfg->db_path);
        GError *repo_err = NULL;
        if (!nostrum_storage_init (relay->storage, &repo_err)) {
                g_critical ("Failed to init relay storage: %s",
                            repo_err ? repo_err->message : "unknown error");
                g_clear_error (&repo_err);
        }
        return relay;
}


void
nostrum_relay_free (NostrumRelay *relay)
{
        if (!relay)
                return;

        g_clear_object (&relay->server);
        nostrum_storage_free (relay->storage);

        nostrum_relay_config_clear (relay->cfg);
        g_free (relay->cfg);
        g_free (relay);
}

gboolean
nostrum_relay_listen (NostrumRelay  *relay,
                      GError       **error)
{
        // Must be true
        g_return_val_if_fail (error == NULL || *error == NULL, 0);
        g_return_val_if_fail (relay != NULL,                   FALSE);
        g_return_val_if_fail (relay->cfg != NULL,              FALSE);
        g_return_val_if_fail (relay->cfg->server_http_port ||
                              relay->cfg->server_https_port,   FALSE);
        
        // FIXME validations on config values
        
        if (relay->cfg->server_https_port != 0) {
                const gchar *f_cert = relay->cfg->server_tls_cert;
                const gchar *f_key = relay->cfg->server_tls_key;
                if (g_file_test (f_cert, G_FILE_TEST_EXISTS) &&
                    g_file_test (f_key, G_FILE_TEST_EXISTS)) {
                        g_info ("Loading TLS certificate file ...");
                        g_autoptr (GTlsCertificate) cert =
                          g_tls_certificate_new_from_files (f_cert,
                                                            f_key,
                                                            error);

                        if (!cert) {
                                g_critical ("Error loading TLS: %s",
                                            (*error)->message);
                                return FALSE;
                        }

                        soup_server_set_tls_certificate (relay->server, cert);

                        if (!listen_on_host (relay->server,
                                             relay->cfg->server_host,
                                             relay->cfg->server_https_port,
                                             SOUP_SERVER_LISTEN_HTTPS,
                                             error)) {
                                g_critical("Listen TLS: %s", (*error)->message);
                                return FALSE;
                        }

                        g_info ("Started server on https://0.0.0.0:%d/  "
                                "(NIP-11 with Accept: application/nostr+json)",
                                relay->cfg->server_http_port);
                        g_info("Started server on wss://0.0.0.0:%d/",
                                relay->cfg->server_https_port);

                } else {
                        g_warning ("Missing PEM files: unable to start HTTPS");
                }
        }

        if (relay->cfg->server_http_port != 0) {
                if (!listen_on_host (relay->server,
                                     relay->cfg->server_host,
                                     relay->cfg->server_http_port,
                                     0,
                                     error)) {
                        g_critical ("Listen error: %s", (*error)->message);
                        return FALSE;
                }

                g_info ("Started server on http://0.0.0.0:%d/  "
                        "(NIP-11 with Accept: application/nostr+json)",
                        relay->cfg->server_http_port);
                g_info ("Started server on ws://0.0.0.0:%d",
                        relay->cfg->server_http_port);
        }

        return TRUE;
}

// =============================================================================
// HANDLE MESSAGE FUNCTIONS
// =============================================================================


static void
process_nip09(NostrumStorage *storage, const NostrumEvent *event)
{
        if (nostrum_event_get_kind (event) != 5)
                return;
        
        g_debug ("Processing NIP-09 event id=%s", nostrum_event_get_id (event));
        
        const char *evt_pubkey = nostrum_event_get_pubkey(event);
        gint64 evt_created_at = nostrum_event_get_created_at(event);

        GPtrArray *e_tags  = g_ptr_array_new_with_free_func (g_free);
        GPtrArray *a_tags  = g_ptr_array_new_with_free_func (g_free);
        GArray    *k1_tags = g_array_sized_new (FALSE, FALSE, sizeof (gint), 0);
        GArray    *k2_tags = g_array_sized_new (FALSE, FALSE, sizeof (gint), 0);

        // Read tag values from event to build filters -------------------------
        const GPtrArray *tags = nostrum_event_get_tags (event);
        for (guint i = 0; i < tags->len; i++) {
                GPtrArray *tag = g_ptr_array_index (tags, i);
                if (tag->len < 2)
                        continue;
                const char *tag_type = g_ptr_array_index (tag, 0);

                if (g_strcmp0 (tag_type, "e") == 0) {
                        const char *value = g_ptr_array_index (tag, 1);
                        g_ptr_array_add (e_tags, g_strdup (value));

                } else if (g_strcmp0 (tag_type, "a") == 0) {
                        const char *value = g_ptr_array_index (tag, 1);
                        g_ptr_array_add (a_tags, g_strdup (value));
                        
                } else if (g_strcmp0 (tag_type, "k") == 0) {
                        const char *value = g_ptr_array_index (tag, 1);
                        char *end = NULL;
                        gint v = (gint) strtol (value, &end, 10);
                        if (!end || *end)
                                continue;
                        g_array_append_val (k1_tags, v);
                        g_array_append_val (k2_tags, v);
                }
        }

        // Build filters -------------------------------------------------------
        NostrumFilter *f1 = nostrum_filter_new ();
        nostrum_filter_take_kinds (f1, k1_tags);
        nostrum_filter_take_ids(f1, e_tags);
        if (!nostrum_filter_is_empty(f1)) {
                nostrum_filter_set_until(f1, evt_created_at);
        }

        NostrumFilter *f2 = nostrum_filter_new ();
        nostrum_filter_take_kinds (f2, k2_tags);
        nostrum_filter_take_dedup_keys(f2, a_tags);
        if (!nostrum_filter_is_empty(f2)) {
                nostrum_filter_set_until(f2, evt_created_at);
        }

        g_autoptr(GPtrArray) filters =
           g_ptr_array_new_with_free_func ((GDestroyNotify)nostrum_filter_free);
        
        g_ptr_array_add (filters, f1);
        g_ptr_array_add (filters, f2);

        // Search for matching events ------------------------------------------
        GError *err = NULL;
        g_autoptr(GPtrArray) events = nostrum_storage_search (storage,
                                                              filters,
                                                              &err);

        if (err) {
                g_warning ("Error searching for events matching "
                           "NIP-09 filters: %s", err->message);
                g_clear_error (&err);
                return;
        }
        g_debug ("Found %d events matching NIP-09 filters", events->len);
        for (gint i = events->len - 1; i >= 0; i--) {
                NostrumEvent *e = g_ptr_array_index (events, i);
                if (g_strcmp0(nostrum_event_get_pubkey(e), evt_pubkey) != 0) {
                        g_debug("Skipping event id=%s kind=%d storage_id=%"
                                G_GINT64_FORMAT " because pubkey does not "
                                "match event pubkey",
                                nostrum_event_get_id(e),
                                nostrum_event_get_kind(e),
                                nostrum_event_get_storage_id(e));
                        g_ptr_array_remove_index_fast(events, i);
                        continue;
                }
                g_debug ("NIP-09 matched event id=%s kind=%d "
                         "storage_id=%" G_GINT64_FORMAT,
                         nostrum_event_get_id (e),
                         nostrum_event_get_kind (e),
                         nostrum_event_get_storage_id (e));
        }

        guint deleted = nostrum_storage_delete (storage, events, &err);
        if (err) {
                g_warning ("Error deleting events matching NIP-09 filters: %s",
                           err->message);
                g_clear_error (&err);
                return;
        }
        g_debug ("NIP-09 Deleted %d events", deleted);
}


static void
handle_event (SoupWebsocketConnection *conn,
              JsonArray               *arr)
{
        // Expected: ["EVENT", {event}]

        g_debug ("Handling EVENT");

        // VALIDATIONS ---------------------------------------------------------

        if (json_array_get_length (arr) < 2) {
                g_debug ("No event");
                msg_response_to_event (conn, NULL, FALSE, "no event provided");
                return;
        }

        JsonNode *event_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_OBJECT (event_node)) {
                msg_response_to_event (conn, NULL, FALSE, "payload must be an "
                                                          "object");
                return;
        }

        g_autofree char *event_json_str =
                nostrum_json_utils_node_to_str (event_node);
        if (!event_json_str) {
                msg_response_to_event (conn, NULL, FALSE, "payload must be an "
                                                          "object");
                return;
        }

        GError *err = NULL;
        g_autoptr(NostrumEvent) event = nostrum_event_from_json (event_json_str,
                                                                 &err);
        if (!event) {
                g_debug ("Failed to convert to NostrumEvent: %s",
                         err ? err->message : "unknown error");
                g_clear_error (&err);
                msg_response_to_event (conn, NULL, FALSE, "error converting "
                                                          "event to object");
                return;
        }

        // Checking signature --------------------------------------------------
        if (!nostrum_event_verify_sig (event, &err)) {
                g_debug ("Invalid event signature: %s", err ? err->message
                                                            : "unknown error");
                g_clear_error (&err);
                msg_response_to_event (conn,
                                       event,
                                       FALSE,
                                       "invalid: bad signature");
                return;
        
        }

        // Persist event in DB -------------------------------------------------
        NostrumRelay *relay = g_object_get_data (G_OBJECT (conn),
                                                 "nostrum-relay");
        g_debug ("Persisting event id=%s kind=%d created_at=%ld",
                 nostrum_event_get_id (event),
                 nostrum_event_get_kind (event),
                 nostrum_event_get_created_at (event));

        if (!nostrum_storage_save (relay->storage, event, &err)) {
                g_debug ("Failed to persist event: %s",
                         err ? err->message : "unknown error");
                
                const gchar *reason = "error: failed to persist event";
                gboolean accepted = FALSE;

                // FIXME
                if (err->code == NOSTRUM_STORAGE_ERROR_DUPLICATE) {
                        reason = "duplicate: already have this event";
                        accepted = TRUE;
                } else if (err->code == NOSTRUM_STORAGE_ERROR_NEWER_EXISTS) {
                        reason = "replaced: have newer event";
                        accepted = FALSE;
                } else if (err->code == NOSTRUM_STORAGE_ERROR_ALREADY_DELETED) {
                        reason = "deleted: user requested deletion";
                        accepted = FALSE;
                }

                msg_response_to_event (conn, event, accepted, reason);
                g_clear_error (&err);
                return;
        }

        process_nip09(relay->storage, event);

        // Send OK (accepted) --------------------------------------------------
        msg_response_to_event (conn, event, TRUE, "");
        
        // Forward event to matching subscriptions -----------------------------
        g_debug ("Checking subscriptions for event id=%s",
                 nostrum_event_get_id (event));        
        for (GList *node = connections_list; node != NULL; node = node->next) {
                if (conn == node->data)
                        continue;
                SoupWebsocketConnection *iconn =
                    (SoupWebsocketConnection *)node->data;
                GPtrArray *subs = get_subscriptions_from_connection (iconn);
                g_debug ("Checking subscriptions (total %d) for connection %p",
                         subs->len, iconn);
                for (guint i = 0; i < subs->len; i++) {
                        NostrumSubscription *sub = g_ptr_array_index (subs, i);
                        if (nostrum_subscription_matches_event (sub, event)) {
                                g_debug ("Event matches subscription id=%s",
                                         nostrum_subscription_get_id (sub));
                                msg_event_to_subscriber (iconn, sub, event);
                        }
                }
        }
}

// Returns the subscription (transfer none)
static const NostrumSubscription *
add_or_replace_subscription(SoupWebsocketConnection  *conn,
                            const NostrumMsgReq      *req,
                            const gchar              *sub_id)
{
        NostrumSubscription *sub = nostrum_subscription_new (sub_id);
        nostrum_subscription_take_filters (sub,
                                           nostrum_msg_req_dup_filters(req));

        GPtrArray *active_subs = get_subscriptions_from_connection (conn);

        // Remove a subscription with the same id if already exists
        for (guint i = 0; i < active_subs->len; i++) {
                NostrumSubscription *existing =
                    g_ptr_array_index (active_subs, i);
                const gchar *existing_id =
                    nostrum_subscription_get_id (existing);
                if (existing_id && g_strcmp0 (existing_id, sub_id) == 0) {
                        g_debug ("Subscription '%s' already exists -> "
                                 "replacing", sub_id);
                        // Free old subscription and remove from array
                        g_ptr_array_remove_index_fast (active_subs, i);
                        break;
                }
        }

        g_ptr_array_add (active_subs, sub);
        g_debug ("Subscription '%s' associated to connection %p", sub_id, conn);

        return sub;
}

static void
handle_req (SoupWebsocketConnection *conn, JsonNode *node, const gchar *sub_id)
{
        // Expected: ["REQ", "sub_id", {filter1}, {filter2}, ...]
        g_debug ("Handling REQ (%s) ...", sub_id);

        NostrumRelay *relay = g_object_get_data (G_OBJECT (conn),
                                                 "nostrum-relay");

        GError        *err = NULL;
        NostrumMsgReq *req = nostrum_msg_req_from_json_node (node, &err);

        // Error parsing REQ, send CLOSED --------------------------------------
        if (!req) {
                gchar *msg_error = "error: parsing req";
                if (g_error_matches (err,
                                     NOSTRUM_FILTER_ERROR,
                                     NOSTRUM_FILTER_UNKNOWN_ELEMENT)) {
                        msg_error = "unsupported: filter contains "
                                    "unknown elements";
                }

                // Send CLOSED to client
                g_autoptr (NostrumMsgClosed) closed = nostrum_msg_closed_new ();
                nostrum_msg_closed_set_subscription_id (closed, sub_id);
                nostrum_msg_closed_set_message (closed, msg_error);
                g_autofree gchar *msg = nostrum_msg_closed_to_json (closed);
                soup_websocket_connection_send_text (conn, msg);
                g_clear_error (&err);

                goto error;
        }
        
        // FIXME when no filter provided ???

        // ADD OR REPLACE SUBSCRIPTION -----------------------------------------

        const NostrumSubscription *sub = add_or_replace_subscription (conn,
                                                                      req,
                                                                      sub_id);

        // Send stored matching events -----------------------------------------
        {
                g_autoptr (GPtrArray) events = NULL;
                const GPtrArray *filters = nostrum_msg_req_get_filters (req);
                events = nostrum_storage_search (relay->storage, filters, &err);
                if (err) {
                        g_warning ("Error searching stored events: %s",
                                err->message ? err->message : "(no message)");
                        g_clear_error (&err);
                        goto error;
                }
                
                g_debug ("Sending stored events (%d) ...", events->len);
                // Send all events found
                for (guint i = 0; i < events->len; i++) {
                        NostrumEvent *ev = g_ptr_array_index (events, i);
                        msg_event_to_subscriber (conn, sub, ev);
                }
        }
        // Send EOSE -----------------------------------------------------------
        {
                g_autoptr (NostrumMsgEose) eose = nostrum_msg_eose_new ();
                nostrum_msg_eose_set_subscription_id (eose, sub_id);
                g_autofree gchar *msg = nostrum_msg_eose_to_json (eose);
                soup_websocket_connection_send_text (conn, msg);
        }
error:
        nostrum_msg_req_free (req);
}

static void
handle_close (SoupWebsocketConnection *conn, JsonArray *arr)
{
        // Expected array: ["CLOSE", "subid"]
        g_debug ("dispatch: CLOSE");
        gsize n = json_array_get_length (arr);
        if (n < 2) {
                g_debug ("CLOSE: array too short");
                return;
        }
        JsonNode *id_node = json_array_get_element (arr, 1);
        if (!JSON_NODE_HOLDS_VALUE(id_node) ||
             json_node_get_value_type(id_node) != G_TYPE_STRING) {
                g_debug("CLOSE: subscription id must be a string");
                return;
        }
        
        const gchar *sub_id = json_node_get_string (id_node);

        GPtrArray *active_subs = get_subscriptions_from_connection (conn);
        for (guint i = 0; i < active_subs->len; i++) {
                NostrumSubscription *sub = g_ptr_array_index (active_subs, i);

                if (!g_strcmp0 (nostrum_subscription_get_id (sub), sub_id)) {
                        g_debug ("WS: closing subscription '%s'", sub_id);
                        g_ptr_array_remove_index_fast (active_subs, i);
                        break;
                }
        }
}

static void
on_ws_message (SoupWebsocketConnection *conn,
               gint                     type,
               GBytes                  *msg,
               gpointer                 user_data)
{

        (void)user_data;

        if (type != SOUP_WEBSOCKET_DATA_TEXT)
                return;

        gsize len = 0;
        const gchar *data = g_bytes_get_data (msg, &len);
        if (!data || len == 0)
                return;

        g_autofree gchar *text = g_strndup (data, len);
        g_debug ("Received raw message: '%s'", text);

        g_autoptr (JsonParser) parser = json_parser_new ();
        GError *perr = NULL;
        if (!json_parser_load_from_data (parser, text, -1, &perr)) {
                g_debug ("JSON: parse error: %s", perr->message);
                g_clear_error (&perr);
                return;
        }

        JsonNode *root = json_parser_get_root (parser);
        if (!JSON_NODE_HOLDS_ARRAY (root)) {
                g_debug ("JSON: root JSON must be an array (NIP-01).");
                return;
        }

        JsonArray *arr = json_node_get_array (root);
        if (json_array_get_length (arr) == 0) {
                g_debug ("JSON: empty message array.");
                return;
        }

        JsonNode *n0 = json_array_get_element (arr, 0);
        if (!JSON_NODE_HOLDS_VALUE (n0) ||
            json_node_get_value_type (n0) != G_TYPE_STRING) {
                g_debug ("JSON: first element must be a string message type.");
                return;
        }

        const gchar *typ = json_node_get_string (n0);
        const gchar *arg1 = NULL;
        if (!typ) {
                g_debug ("JSON: message type is NULL.");
                return;
        }

        if (json_array_get_length (arr) > 1) {
                        JsonNode *n1 = json_array_get_element (arr, 1);
                if (!JSON_NODE_HOLDS_VALUE (n1) ||
                        json_node_get_value_type (n1) == G_TYPE_STRING) {
                                arg1 = json_node_get_string (n1);
                }
        }

        if (g_strcmp0 (typ, "EVENT") == 0) {
                handle_event (conn, arr);
        } else if (g_strcmp0 (typ, "REQ") == 0) {
                handle_req (conn, root, arg1);
        } else if (g_strcmp0 (typ, "CLOSE") == 0) {
                handle_close (conn, arr);
        } else {
                g_debug ("Unknown/unsupported message type: '%s'", typ);
        }
        
        g_debug("Done handling message");
}

// =============================================================================
// WS CALLBACKS FUNCTIONS
// =============================================================================

static void
on_ws_error (SoupWebsocketConnection *conn, GError *error, gpointer u)
{
        const gchar *ip = g_object_get_data(G_OBJECT(conn), "client-ip");
        guint16 port = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (conn),
                                                            "client-port"));

        g_warning ("WS error on conn %s:%d: %s",
                   ip,
                   port,
                   error ? error->message : "unknown");
}

static void
on_ws_closed (SoupWebsocketConnection *conn, gpointer u)
{
        (void)u;
        const gchar *ip = g_object_get_data(G_OBJECT(conn), "client-ip");
        guint16 port = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (conn),
                                                            "client-port"));

        g_info ("WS connection closed: %s:%d", ip, port);
        connections_list = g_list_remove (connections_list, conn);
}

static void
on_ws_connected (SoupServer               *server,
                 SoupServerMessage        *msg,
                 const char               *path,
                 SoupWebsocketConnection  *conn,
                 gpointer                  user_data)
{
        (void)server;
        (void)msg;
        (void)path;

        NostrumRelay *relay = (NostrumRelay *)user_data;


        GSocketAddress *addr = soup_server_message_get_remote_address (msg);
        gchar *ip_str = NULL;
        guint16 port = 0;
        if (G_IS_INET_SOCKET_ADDRESS (addr)) {
                GInetSocketAddress *inet_sock = G_INET_SOCKET_ADDRESS(addr);
                GInetAddress *inet_addr =
                  g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (addr));
                ip_str = g_inet_address_to_string (inet_addr);
                port = g_inet_socket_address_get_port(inet_sock);
        } else {
                ip_str = g_strdup ("unknown");
        }

        g_info ("New connection established from %s:%d", ip_str, port);

        g_object_set_data_full (G_OBJECT(conn), "client-ip", ip_str, g_free);
        g_object_set_data (G_OBJECT (conn),
                           "client-port",
                           GUINT_TO_POINTER(port));

        // keep the connection alive
        g_object_ref (conn);
        g_signal_connect_swapped (conn,
                                  "closed",
                                  G_CALLBACK (g_object_unref),
                                  conn);
        g_signal_connect (conn, "message", G_CALLBACK (on_ws_message), NULL);
        g_signal_connect (conn, "error", G_CALLBACK (on_ws_error), NULL);
        g_signal_connect (conn, "closed", G_CALLBACK (on_ws_closed), NULL);

        g_object_set_data (G_OBJECT (conn), "nostrum-relay", relay);

        connections_list = g_list_append (connections_list, conn);
}


static void
on_nip11 (SoupServer        *server,
          SoupServerMessage *msg,
          const char        *path,
          GHashTable        *query,
          gpointer           user_data)
{
        (void)server;
        (void)path;
        (void)query;

        const struct NostrumRelayConfig *cfg =
            (const struct NostrumRelayConfig *)user_data;

        SoupMessageHeaders *reqh =
            soup_server_message_get_request_headers (msg);

        const char *upgrade = soup_message_headers_get_one (reqh, "Upgrade");

        const char *accept = soup_message_headers_get_one (reqh, "Accept");


        // WEBSOCKET. do nothing here
        if (upgrade && g_ascii_strcasecmp (upgrade, "websocket") == 0) {
                return;
        }

        if (!accept || !(g_strrstr (accept, "application/nostr+json"))) {
                return;
        }

        g_debug ("Responding to NIP-11 request");

        const char *nip11_json = "{"
                "\"name\":\"%s\","
                "\"description\":\"%s\","
                "\"supported_nips\":[1,2,9,11,12,16,20,33],"
                "\"software\":\"nostrum-relay\","
                "\"version\":\"%s\","
                "\"contact\":\"%s\""
                "}";

        soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);

        g_autofree gchar *nip11_json_str = g_strdup_printf (nip11_json,
                                                            cfg->info_name,
                                                            cfg->info_description,
                                                            NOSTRUM_VERSION,
                                                            cfg->info_contact);
        soup_server_message_set_response (msg,
                                          "application/nostr+json",
                                          SOUP_MEMORY_COPY,
                                          nip11_json_str,
                                          strlen(nip11_json_str));

        SoupMessageHeaders *resh =
            soup_server_message_get_response_headers (msg);

        soup_message_headers_replace (resh,
                                      "Access-Control-Allow-Origin",
                                      "*");
}

// =============================================================================
// HELPERS
// =============================================================================

static void
msg_event_to_subscriber(SoupWebsocketConnection        *conn,
                        const NostrumSubscription      *sub,
                        const NostrumEvent             *event)
{
        g_autofree gchar *event_str = nostrum_event_to_json (event);
        if (event_str) {
                // FIXME use json builder
                g_autofree gchar *msg = NULL;
                msg = g_strdup_printf ("[\"EVENT\",\"%s\",%s]",
                                       nostrum_subscription_get_id (sub),
                                       event_str);
                soup_websocket_connection_send_text (conn, msg);
        }
}

static void
msg_response_to_event (SoupWebsocketConnection       *conn,
                       const NostrumEvent            *event,
                       gboolean                      accepted,
                       const gchar                   *reason)
{
        const char *id = (event != NULL) ? nostrum_event_get_id (event)
                                         : "";

        const gchar *msg_reason = reason ? reason : (accepted ? "accepted"
                                                              : "rejected");
        g_autoptr(NostrumMsgOk) ok = nostrum_msg_ok_new ();

        nostrum_msg_ok_set_id (ok, id);
        nostrum_msg_ok_set_accepted (ok, accepted);
        nostrum_msg_ok_set_message (ok, msg_reason);

        g_autofree char *json = nostrum_msg_ok_to_json (ok);
        soup_websocket_connection_send_text (conn, json);
}

static
GPtrArray *get_subscriptions_from_connection (SoupWebsocketConnection *conn)
{
        GPtrArray *subs = g_object_get_data (G_OBJECT (conn),
                                             "nostrum-subscriptions");
        if (!subs) {
                subs = g_ptr_array_new_with_free_func (
                    (GDestroyNotify) nostrum_subscription_free);
                g_object_set_data_full (G_OBJECT (conn),
                                        "nostrum-subscriptions",
                                        subs,
                                        (GDestroyNotify)g_ptr_array_unref);
        }

        return subs;
}

static gboolean
listen_on_host (SoupServer                *server,
                const char                *host,
                guint                      port,
                SoupServerListenOptions    opts,
                GError                   **error)
{
        // Must be true
        g_return_val_if_fail (error == NULL || *error == NULL, 0);


        if (g_strcmp0 (host, "0.0.0.0") == 0 || g_strcmp0 (host, "::") == 0) {
                return soup_server_listen_all(server, port, opts, error);
        }

        if (g_strcmp0 (host, "127.0.0.1") == 0 || g_strcmp0 (host, "::1") == 0){
                return soup_server_listen_local(server, port, opts, error);
        }

        GInetAddress *addr = g_inet_address_new_from_string (host);
        if (!addr) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "Invalid host: %s", host);
                return FALSE;
        }

        GSocketAddress *sockaddr = g_inet_socket_address_new (addr, port);

        gboolean ok = soup_server_listen (server, sockaddr, opts, error);

        g_object_unref (sockaddr);
        g_object_unref (addr);

        return ok;
}