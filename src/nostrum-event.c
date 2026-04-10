/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumEvent object (implementation of nostrum-event.h)                    */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-event.h"
#include "nostrum-utils.h"
#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>


struct _NostrumEvent
{
        char       *id;
        char       *pubkey;
        long        created_at;
        int         kind;
        GPtrArray  *tags;
        char       *content;
        char       *sig;
        long        storage_id; // id in the database, -1 if not stored yet
        char       *dedup_key;  // computed deduplication keys, for NIP-33
};

G_DEFINE_QUARK (nostrum-event-error-quark, nostrum_event_error)


// =============================================================================
// HELPERS -- DECLARATIONS
// =============================================================================

#define JSON_STR(obj, member, def) json_object_get_string_member_with_default \
                                   (obj, member, def)
#define JSON_INT(obj, member, def) json_object_get_int_member_with_default \
                                   (obj, member, def)
#define JSON_OBJ(obj, member)      json_object_get_member(obj, member)
#define JSON_ARRAY(node)           json_node_get_array(node)

static gchar      *compute_id       (const NostrumEvent *e, GError **err);
static void        free_tag_array   (gpointer p);

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumEvent *
nostrum_event_new (void)
{
        NostrumEvent *e = g_new0 (NostrumEvent, 1);
        e->storage_id = -1; // not stored yet
        return e;
}

void
nostrum_event_free (NostrumEvent *e)
{
        if (!e) {
                return;
        }
        g_free (e->id);
        g_free (e->pubkey);
        g_free (e->content);
        g_free (e->sig);
        if (e->tags) 
                g_ptr_array_unref (e->tags);
        g_free (e->dedup_key);
        g_free (e);
}

// =============================================================================
// OPERATIONS
// =============================================================================

void
nostrum_event_compute_id (NostrumEvent *e, GError **err)
{
        g_autofree gchar *id = compute_id (e, err);
        nostrum_event_set_id (e, id);
}

char *
nostrum_event_serialize (const NostrumEvent *e, GError **err)
{
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        if (!e || !e->pubkey || !e->content || !e->tags) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_SERIALIZE,
                             "Cannot serialize incomplete event");
                return NULL;
        }

        JsonBuilder *b = json_builder_new ();
        json_builder_begin_array (b);

        // 0
        json_builder_add_int_value (b, 0);

        // pubkey (string)
        json_builder_add_string_value (b, e->pubkey);

        // created_at (int)
        json_builder_add_int_value (b, (gint64)e->created_at);

        // kind (int)
        json_builder_add_int_value (b, (gint64)e->kind);

        // tags: array de arrays de strings
        json_builder_begin_array (b);
        for (guint i = 0; e->tags && i < e->tags->len; i++) {
                GPtrArray *tag = g_ptr_array_index (e->tags, i);
                json_builder_begin_array (b);
                for (guint j = 0; tag && j < tag->len; j++) {
                        const char *field = g_ptr_array_index (tag, j);
                        json_builder_add_string_value (b, field ? field
                                                                : "");
                }
                json_builder_end_array (b);
        }
        json_builder_end_array (b);

        // content (string)
        json_builder_add_string_value (b, e->content);

        json_builder_end_array (b);

        // to compact string (UTF-8)
        g_autoptr (JsonGenerator) g = json_generator_new ();
        g_autoptr (JsonNode) root = json_builder_get_root (b);
        json_generator_set_root (g, root);
        char *out = json_generator_to_data (g, NULL); // caller must free
        g_object_unref (b);
        return out;
}

gboolean
nostrum_event_verify_sig (const NostrumEvent *e, GError **err)
{
        g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

        if (!e || !e->pubkey || !e->sig) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_VERIFY,
                             "no pubkey or sig in event");
                return FALSE;
        }

        // 1. Serialize event
        g_autofree char *ser = nostrum_event_serialize (e, err);
        g_return_val_if_fail (ser != NULL, FALSE);

        guint8 msg32[32];
        if (!nostrum_utils_sha256_to_bytes (ser, -1, msg32)) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_SERIALIZE,
                             "failed to compute sha256 of serialized event");
                return FALSE;
        }

        // 2. Parse pubkey (32 bytes x-only) and sig (64 bytes)
        guint8 pk32[32];
        guint8 sig64[64];
        if (!nostrum_utils_hex_to_bytes (e->pubkey, pk32, 32)) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_BAD_PUBKEY,
                             "invalid pubkey (64 hex expected)");
                return FALSE;
        }
        if (!nostrum_utils_hex_to_bytes (e->sig, sig64, 64)) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_BAD_SIG,
                             "invalid sig (128 hex expected)");
                return FALSE;
        }

        // 3. Create secp256k1 context
        secp256k1_context *ctx =
            secp256k1_context_create (SECP256K1_CONTEXT_VERIFY);
        if (!ctx) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_VERIFY,
                             "failed to create secp256k1 context");
                return FALSE;
        }
        secp256k1_xonly_pubkey xpub;
        int ok = secp256k1_xonly_pubkey_parse (ctx, &xpub, pk32);
        if (!ok) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_BAD_PUBKEY,
                             "failed to parse xonly pubkey");
                secp256k1_context_destroy (ctx);
                return FALSE;
        }

        // 4. Verify signature
        ok = secp256k1_schnorrsig_verify (ctx, sig64, msg32, 32, &xpub);
        secp256k1_context_destroy (ctx);

        if (!ok) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_BAD_SIG,
                             "signature verification failed");
                return FALSE;
        }
        return TRUE;
}

void
nostrum_event_add_tag (NostrumEvent *e, const char *str, ...)
{
        g_return_if_fail (e != NULL);

        if (!e->tags) {
                e->tags = g_ptr_array_new_with_free_func (free_tag_array);
        }

        GPtrArray *tag_arr = g_ptr_array_new_with_free_func (g_free);
        if (str) {
                g_ptr_array_add (tag_arr, g_strdup (str));
        }
        va_list ap;
        va_start (ap, str);
        const char *s;
        while ((s = va_arg (ap, const char *)) != NULL) {
                g_ptr_array_add (tag_arr, g_strdup (s));
        }
        va_end (ap);

        g_ptr_array_add (e->tags, tag_arr);
}

gboolean
nostrum_event_is_addressable (const NostrumEvent *e)
{
        g_return_val_if_fail (e != NULL, FALSE);
        int kind = nostrum_event_get_kind (e);
        return (kind >= 30000 && kind < 40000);
}

gboolean
nostrum_event_is_ephemeral (const NostrumEvent *e)
{
        g_return_val_if_fail (e != NULL, FALSE);
        int kind = nostrum_event_get_kind (e);
        return (kind >= 20000 && kind < 30000);
}

gboolean
nostrum_event_is_replaceable (const NostrumEvent *e)
{
        g_return_val_if_fail (e != NULL, FALSE);
        int kind = nostrum_event_get_kind (e);
        return (kind >= 10000 && kind < 20000) || kind == 0 || kind == 3;
}

gboolean
nostrum_event_is_regular (const NostrumEvent *e)
{
        g_return_val_if_fail (e != NULL, FALSE);
        int kind = nostrum_event_get_kind (e);
        return (kind >= 1000 && kind < 10000) || (kind >= 4 && kind < 45) ||
                kind == 1 || kind == 2;
}


// =============================================================================
// JSON CONVERSIONS
// =============================================================================

NostrumEvent *
nostrum_event_from_json (const char   *json,
                         GError      **err)
{
        g_return_val_if_fail (json != NULL, NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        NostrumEvent *event = NULL;
        GPtrArray *tmp_tags = NULL;

        // Parsing JSON string
        g_autoptr(JsonParser) p = json_parser_new();
        g_autoptr(GError) json_err = NULL;
        if (!json_parser_load_from_data(p, json, -1, &json_err)) {
                g_set_error(err,
                            NOSTRUM_EVENT_ERROR,
                            NOSTRUM_EVENT_ERROR_PARSE,
                            "Error parsing event JSON: (%s, code=%d): %s",
                            g_quark_to_string(json_err->domain),
                            json_err->code,
                            json_err->message);
                goto error;
        }

        // Checking JSON object
        JsonNode *root = json_parser_get_root (p);
        if (!JSON_NODE_HOLDS_OBJECT (root)) {
                g_set_error (err,
                             NOSTRUM_EVENT_ERROR,
                             NOSTRUM_EVENT_ERROR_PARSE,
                             "Event JSON must be an object");
                goto error;
        }

        JsonObject *js_obj = json_node_get_object (root);

        // Reading fields from json obj
        const char *id      = JSON_STR (js_obj, "id", NULL);
        const char *pubkey  = JSON_STR (js_obj, "pubkey", NULL);
        const char *content = JSON_STR (js_obj, "content", NULL);
        const char *sig     = JSON_STR (js_obj, "sig", NULL);
        gint64 created_at   = JSON_INT (js_obj, "created_at", -1);
        gint64 kind         = JSON_INT (js_obj, "kind", -1);

        // Checking tags array
        JsonArray *tags_arr = NULL;
        if (json_object_has_member (js_obj, "tags")) {
                JsonNode *n_tags = JSON_OBJ (js_obj, "tags");
                if (!JSON_NODE_HOLDS_ARRAY (n_tags)) {
                        g_set_error (err,
                                     NOSTRUM_EVENT_ERROR,
                                     NOSTRUM_EVENT_ERROR_PARSE,
                                     "'tags' must be an array");
                        goto error;
                }
                tags_arr = JSON_ARRAY (n_tags);
        }

        // Creating NostrumEvent
        event = nostrum_event_new ();
        event->id         = g_strdup (id);
        event->pubkey     = g_strdup (pubkey);
        event->sig        = g_strdup (sig);
        event->content    = g_strdup (content);
        event->kind       = (int)kind;
        event->created_at = (time_t)created_at;

        tmp_tags = g_ptr_array_new_with_free_func (free_tag_array);

        guint tags_arr_len = json_array_get_length (tags_arr);
        for (guint i = 0; i < tags_arr_len; i++) {
                JsonNode *tn = json_array_get_element (tags_arr, i);
                if (!JSON_NODE_HOLDS_ARRAY (tn)) {
                        g_set_error (err,
                                     NOSTRUM_EVENT_ERROR,
                                     NOSTRUM_EVENT_ERROR_PARSE,
                                     "tag[%u] must be an array",
                                     i);
                        goto error;
                }
                
                JsonArray *inner_arr = JSON_ARRAY (tn);
                g_autoptr (GPtrArray) inner =
                    g_ptr_array_new_with_free_func (g_free);

                guint inner_len = json_array_get_length (inner_arr);

                for (guint j = 0; j < inner_len; j++) {
                        JsonNode *f = json_array_get_element (inner_arr, j);
                        if (!JSON_NODE_HOLDS_VALUE (f) || 
                            json_node_get_value_type (f) != G_TYPE_STRING) {
                                g_set_error (err,
                                             NOSTRUM_EVENT_ERROR,
                                             NOSTRUM_EVENT_ERROR_PARSE,
                                             "Tag[%u][%u] must be string",
                                             i,
                                             j);
                                goto error;
                        }
                        const char *s = json_node_get_string (f);
                        g_ptr_array_add (inner, g_strdup (s ? s : ""));
                }
                g_ptr_array_add (tmp_tags, g_steal_pointer (&inner));
        }

        nostrum_event_take_tags (event, tmp_tags);
        return event;

error:
        nostrum_event_free (event);
        if (tmp_tags)
                g_ptr_array_unref (tmp_tags);
        return NULL;
}


gchar *
nostrum_event_to_json (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);

        g_autoptr(JsonBuilder) b = json_builder_new ();

        json_builder_begin_object (b);

        json_builder_set_member_name (b, "id");
        json_builder_add_string_value (b, event->id);

        json_builder_set_member_name (b, "pubkey");
        json_builder_add_string_value (b, event->pubkey);

        json_builder_set_member_name (b, "created_at");
        json_builder_add_int_value (b, event->created_at);

        json_builder_set_member_name (b, "kind");
        json_builder_add_int_value (b, event->kind);

        json_builder_set_member_name (b, "tags");
        json_builder_begin_array (b);
        for (guint i = 0; i < event->tags->len; i++) {
                GPtrArray *tag = g_ptr_array_index (event->tags, i);

                json_builder_begin_array (b);
                for (guint j = 0; j < tag->len; j++) {
                        const gchar *s = g_ptr_array_index (tag, j);
                        json_builder_add_string_value (b, s);
                }
                json_builder_end_array (b);
        }
        json_builder_end_array (b);

        json_builder_set_member_name (b, "content");
        json_builder_add_string_value (b, event->content);

        json_builder_set_member_name (b, "sig");
        json_builder_add_string_value (b, event->sig);

        json_builder_end_object (b);

        g_autoptr(JsonGenerator) gen = json_generator_new ();
        JsonNode *root = json_builder_get_root (b);
        json_generator_set_root (gen, root);

        gchar *json_str = json_generator_to_data (gen, NULL);

        json_node_free (root);
        return json_str;
}


void
nostrum_event_print (const NostrumEvent *e)
{
        printf ("{\n");
        printf ("  \"id\": \"%s\",\n", e->id);
        printf ("  \"pubkey\": \"%s\",\n", e->pubkey);
        printf ("  \"created_at\": %ld,\n", e->created_at);
        printf ("  \"kind\": %d,\n", e->kind);
        printf ("  \"tags\": [\n");
        for (guint i = 0; i < e->tags->len; i++) {
                GPtrArray *tag = g_ptr_array_index (e->tags, i);
                printf ("    [");
                for (guint j = 0; j < tag->len; j++) {
                        const char *s = g_ptr_array_index (tag, j);
                        printf ("%s\"%s\"", (j ? ", " : ""), s);
                }
                printf ("]%s\n", (i + 1 < e->tags->len) ? "," : "");
        }
        printf ("  ],\n");
        printf ("  \"content\": \"%s\",\n", e->content);
        printf ("  \"sig\": \"%s\"\n", e->sig);
        printf ("}\n");
}

// =============================================================================
// GETTERS
// =============================================================================

const char *
nostrum_event_get_id (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);
        return event->id;
}

gint64
nostrum_event_get_storage_id (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, -1);
        return event->storage_id;
}

const char *
nostrum_event_get_pubkey (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);
        return event->pubkey;
}

long
nostrum_event_get_created_at (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, 0);
        return event->created_at;
}

int
nostrum_event_get_kind (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, 0);
        return event->kind;
}

const char *
nostrum_event_get_content (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);
        return event->content;
}

const char *
nostrum_event_get_sig (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);
        return event->sig;
}

const GPtrArray *
nostrum_event_get_tags (const NostrumEvent *e)
{
        g_return_val_if_fail (e != NULL, NULL);
        return e->tags;
}

const char *
nostrum_event_get_dedup_key (const NostrumEvent *event)
{
        g_return_val_if_fail (event != NULL, NULL);
        return event->dedup_key;
}


// =============================================================================
// SETTERS
// =============================================================================

void
nostrum_event_set_id (NostrumEvent *event, const char *id)
{
        g_return_if_fail (event != NULL);
        g_return_if_fail (id == NULL || nostrum_utils_is_hex_len (id, 64));

        g_free (event->id);
        event->id = id ? g_strdup (id) : NULL;
}

void
nostrum_event_set_storage_id (NostrumEvent *event, gint64 storage_id)
{
        g_return_if_fail (event != NULL);
        event->storage_id = storage_id;
}

void
nostrum_event_set_pubkey (NostrumEvent *event, const char *pubkey)
{
        g_return_if_fail (event != NULL);
        g_return_if_fail (pubkey == NULL ||
                          nostrum_utils_is_hex_len (pubkey, 64));

        g_free (event->pubkey);
        event->pubkey = pubkey ? g_strdup (pubkey) : NULL;
}

void
nostrum_event_set_created_at (NostrumEvent *event, long created_at)
{
        g_return_if_fail (event != NULL);
        event->created_at = created_at;
}

void
nostrum_event_set_kind (NostrumEvent *event, int kind)
{
        g_return_if_fail (event != NULL);
        event->kind = kind;
}

void
nostrum_event_set_content (NostrumEvent *event, const char *content)
{
        g_return_if_fail (event != NULL);

        g_free (event->content);
        event->content = content ? g_strdup (content) : NULL;
}

void
nostrum_event_set_sig (NostrumEvent *event, const char *sig)
{
        g_return_if_fail (event != NULL);
        g_return_if_fail (sig == NULL || nostrum_utils_is_hex_len (sig, 128));

        g_free (event->sig);
        event->sig = sig ? g_strdup (sig) : NULL;
}


void
nostrum_event_take_tags (NostrumEvent *event, GPtrArray *tags)
{
        g_return_if_fail (event != NULL);

        g_clear_pointer (&event->tags, g_ptr_array_unref);

        event->tags = tags;
}

void
nostrum_event_set_dedup_key (NostrumEvent *event, const char *dedup_key)
{
        g_return_if_fail (event != NULL);

        g_free (event->dedup_key);
        event->dedup_key = dedup_key ? g_strdup (dedup_key) : NULL;
}


// =============================================================================
// HELPERS - IMPLEMENTATION
// =============================================================================


static void
free_tag_array (gpointer p)
{
        g_ptr_array_free ((GPtrArray *)p, TRUE);
}


static gchar *
compute_id (const NostrumEvent *e, GError **err)
{
        g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

        if (!e) {
                return NULL;
        }
        g_autofree char *ser = nostrum_event_serialize (e, err);
        if (!ser) {
                return NULL;
        }
        g_autofree char *id = nostrum_utils_sha256_hex_lower (ser);
        if (!id) {
                return NULL;
        }

        return g_strdup (id);
}
