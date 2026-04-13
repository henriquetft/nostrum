/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* NostrumStorage. Implementation of nostrum-storage.h using sqlite3          */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-storage.h"
#include "nostrum-event.h"
#include "nostrum-filter.h"
#include <glib.h>
#include <sqlite3.h>

#include <stdio.h>

#define G_LOG_DOMAIN "nostrum-storage"

G_DEFINE_QUARK (nostrum-storage-error-quark, nostrum_storage_error)

struct _NostrumStorage
{
        gchar    *db_file;
        sqlite3  *db;
};

// Tag clauses (per filter)
// Each filter has GHashTable of tag_key -> GPtrArray of possible tag_values
// Remember that only the first value in any given tag is indexed
typedef struct
{
        const char       *key;  // borrowed
        const GPtrArray  *vals; // borrowed
} TagClause;

// =============================================================================
// HELPERS -- DECLARATIONS
// =============================================================================

// Returns "(?, ?, ?)" etc
static void
append_in_list_placeholders (GString *s, guint n);

static void
set_sqlite_error (sqlite3       *db,
                  const char    *where,
                  int            rc,
                  gint           code,
                  GError       **err);

static gboolean
exec_sql_or_set_error (sqlite3     *db,
                       const char  *sql,
                       gint         code,
                       GError     **err);

static gint
cmp_str (gconstpointer a, gconstpointer b);

// Returns GPtrArray of TagClause*, sorted by tag key
static GPtrArray *
filter_build_sorted_tag_clauses (const NostrumFilter *f);

// Returns GPtrArray of GPtrArray of TagClause*
static 
GPtrArray *
get_tag_clauses_per_filter (const GPtrArray *filters);

static void
append_tag_exists_clause (GString *sql, guint n_values);

// =============================================================================
// CONSTRUCTORS / DESTRUCTORS
// =============================================================================

NostrumStorage *
nostrum_storage_new (const gchar *db_file)
{
        g_return_val_if_fail (db_file != NULL, NULL);

        NostrumStorage *storage = g_new0 (NostrumStorage, 1);
        storage->db_file = g_strdup (db_file);
        storage->db      = NULL;
        return storage;
}

void
nostrum_storage_free (NostrumStorage *storage)
{
        if (!storage)
                return;

        if (storage->db) {
                sqlite3_close (storage->db);
                storage->db = NULL;
        }

        g_free (storage->db_file);
        g_free (storage);
}

static const gchar *
get_dedup_key(const NostrumEvent *event)
{
        g_autoptr(GString) str = g_string_new ("");
        gint kind = nostrum_event_get_kind(event);
        g_autofree gchar *pubkey = g_strdup(nostrum_event_get_pubkey(event));

        if (nostrum_event_is_addressable(event)) {
                g_string_append_printf(str, "%d:%s:", kind, pubkey);
                const GPtrArray *tags = nostrum_event_get_tags(event);
                if (tags) {
                        for (guint i = 0; i < tags->len; i++) {
                                const GPtrArray *tag_arr =
                                    g_ptr_array_index ((GPtrArray *)tags, i);
                                if (!tag_arr || tag_arr->len < 2)
                                        continue;
                                // tag name and tag value
                                const char *tn = g_ptr_array_index (tag_arr, 0);
                                const char *tv = g_ptr_array_index (tag_arr, 1);

                                if (g_strcmp0(tn, "d") == 0 && tv) {
                                        g_string_append(str, tv);
                                        break;
                                }
                        }
                }
        } else if (nostrum_event_is_replaceable(event)) {
                g_string_append_printf(str, "%d:%s", kind, pubkey);
        }

        if (str->len == 0) {
                return NULL;
        }
        return g_strdup(str->str);
};

static gboolean
is_deleted(NostrumStorage *storage, const NostrumEvent *event)
{
        // Preconditions (Must be true)
        g_return_val_if_fail (storage     != NULL,   FALSE);
        g_return_val_if_fail (storage->db != NULL,   FALSE);
        
        gboolean found = FALSE;
        int rc;
        const char *event_id = nostrum_event_get_id(event);

        sqlite3_stmt *stmt = NULL;

        const char *sql_get_id = "SELECT id FROM events WHERE event_id = ? "
                                 "AND is_deleted = 1;";
        rc = sqlite3_prepare_v2 (storage->db, sql_get_id, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                g_warning("Error preparing statement to check "
                          "if event is deleted: %d", rc);
                goto end;
        }

        g_debug("SQL: (%s)\n", event_id);
        g_debug("Checking if event is deleted with id: %s\n", event_id);

        sqlite3_bind_text (stmt, 1, event_id, -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW) {
                found = TRUE;
        } else if (rc == SQLITE_DONE) {
                found = FALSE;
        } else {
                g_warning("Error executing statement to check "
                          "if event is deleted: %d", rc);
                goto end;
        }

end:
        if (stmt) {
                sqlite3_finalize(stmt);
                stmt = NULL;
        }

        return found;
}
// =============================================================================
// OPERATIONS
// =============================================================================

gboolean
nostrum_storage_init (NostrumStorage *storage, GError **err)
{
        // Preconditions
        g_return_val_if_fail (err == NULL || *err == NULL,    FALSE);
        g_return_val_if_fail (storage != NULL,                FALSE);
        g_return_val_if_fail (storage->db_file != NULL,       FALSE);
        g_return_val_if_fail (storage->db == NULL,            FALSE);

        int rc = sqlite3_open (storage->db_file, &storage->db);
        if (rc != SQLITE_OK) {
                g_set_error (err,
                             NOSTRUM_STORAGE_ERROR,
                             NOSTRUM_STORAGE_ERROR_INIT,
                             "sqlite3_open failed: rc=%d: %s",
                             rc,
                             storage->db ? sqlite3_errmsg (storage->db)
                                         : "(no db)");
                goto error;
        }

        if (!exec_sql_or_set_error (storage->db, "PRAGMA foreign_keys=ON;",
                                    NOSTRUM_STORAGE_ERROR_INIT, err))
                goto error;

        if (!exec_sql_or_set_error (storage->db, "PRAGMA journal_mode=WAL;",
                                    NOSTRUM_STORAGE_ERROR_INIT, err))
                goto error;

        if (!exec_sql_or_set_error (storage->db, "PRAGMA synchronous=NORMAL;",
                                    NOSTRUM_STORAGE_ERROR_INIT, err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE TABLE IF NOT EXISTS events ("
                    "  id          INTEGER    PRIMARY KEY,"
                    "  event_id    TEXT       NOT NULL UNIQUE,"
                    "  pubkey      TEXT       NOT NULL,"
                    "  created_at  INTEGER    NOT NULL,"
                    "  kind        INTEGER    NOT NULL,"
                    "  content     TEXT       NOT NULL,"
                    "  sig         TEXT       NOT NULL,"
                    "  raw_json    TEXT       NOT NULL,"
                    "  dedup_key   TEXT       UNIQUE,"
                    "  is_deleted  INTEGER    NOT NULL DEFAULT 0"
                    ");",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE UNIQUE INDEX IF NOT EXISTS idx_events_event_id "
                    "ON events(event_id);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE INDEX IF NOT EXISTS idx_events_pubkey_created_at "
                    "ON events(pubkey, created_at);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE INDEX IF NOT EXISTS idx_events_kind_created_at "
                    "ON events(kind, created_at);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE INDEX IF NOT EXISTS idx_events_created_at "
                    "ON events(created_at);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE TABLE IF NOT EXISTS event_tags ("
                    "  event_fk  INTEGER NOT NULL,"
                    "  tag       TEXT    NOT NULL,"
                    "  value     TEXT    NOT NULL,"
                    "  pos       INTEGER NOT NULL,"
                    "  PRIMARY KEY (event_fk, tag, value, pos),"
                    "  FOREIGN KEY (event_fk) REFERENCES events(id) "
                    "  ON DELETE CASCADE"
                    ");",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE INDEX IF NOT EXISTS idx_event_tags_tag_value "
                    "ON event_tags(tag, value);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        if (!exec_sql_or_set_error (
                    storage->db,
                    "CREATE INDEX IF NOT EXISTS idx_event_tags_event_fk "
                    "ON event_tags(event_fk);",
                    NOSTRUM_STORAGE_ERROR_INIT,
                    err))
                goto error;

        return TRUE;

error:
        if (storage->db) {
                sqlite3_close (storage->db);
                storage->db = NULL;
        }
        return FALSE;
}
// FIXME ADD INDEXES


gboolean
nostrum_storage_save (NostrumStorage             *storage,
                      NostrumEvent               *event,
                      GError                    **err)
{
        // Preconditions
        g_return_val_if_fail (storage     != NULL,                 FALSE);
        g_return_val_if_fail (storage->db != NULL,                 FALSE);
        g_return_val_if_fail (event       != NULL,                 FALSE);
        g_return_val_if_fail (err         == NULL || *err == NULL, FALSE);


        const char  *event_id   = nostrum_event_get_id (event);
        const char  *pubkey     = nostrum_event_get_pubkey (event);
        gint64       created_at = (gint64)nostrum_event_get_created_at (event);
        gint         kind       = nostrum_event_get_kind (event);
        const char  *content    = nostrum_event_get_content (event);
        const char  *sig        = nostrum_event_get_sig (event);

        g_autofree gchar *dedup_key = get_dedup_key(event);

        g_autofree char *raw_json = nostrum_event_to_json (event);
        if (!raw_json) {
                g_set_error (err,
                             NOSTRUM_STORAGE_ERROR,
                             NOSTRUM_STORAGE_ERROR_SAVE,
                             "event_to_json returned NULL");
                goto error;
        }

        sqlite3_stmt *stmt = NULL;
        int rc = SQLITE_OK;


        if (!exec_sql_or_set_error (storage->db,
                                    "BEGIN;",
                                    NOSTRUM_STORAGE_ERROR_SAVE,
                                    err))
                goto error_sql;

        // INSERT OR UPDATE event ----------------------------------------------
        const char *sql_event = "INSERT INTO events (event_id, pubkey, "
                                "created_at, kind, content, sig, raw_json, "
                                "dedup_key) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                                "ON CONFLICT(dedup_key) DO UPDATE SET "
                                "   event_id = excluded.event_id, "
                                "   content = excluded.content, "
                                "   created_at = excluded.created_at, "
                                "   raw_json = excluded.raw_json, "
                                "   sig = excluded.sig "
                                "WHERE excluded.created_at > events.created_at "
                                "OR (excluded.created_at = events.created_at "
                                "AND excluded.event_id < events.event_id);";

        rc = sqlite3_prepare_v2 (storage->db, sql_event, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                set_sqlite_error(storage->db,
                                 "sqlite3_prepare_v2(save event)",
                                 rc,
                                 NOSTRUM_STORAGE_ERROR_SAVE,
                                 err);
                goto error_sql;
        }

        sqlite3_bind_text (stmt, 1, event_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, pubkey, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64 (stmt, 3, created_at);
        sqlite3_bind_int (stmt, 4, kind);
        sqlite3_bind_text (stmt, 5, content, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 6, sig, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 7, raw_json, -1, SQLITE_TRANSIENT);
        if (dedup_key == NULL) {
                sqlite3_bind_null (stmt, 8);
        } else {
                sqlite3_bind_text (stmt, 8, dedup_key, -1, SQLITE_TRANSIENT);
        }

        rc = sqlite3_step (stmt);

        if (rc != SQLITE_DONE) {
                if (rc == SQLITE_CONSTRAINT ||
                    rc == SQLITE_CONSTRAINT_PRIMARYKEY ||
                    rc == SQLITE_CONSTRAINT_UNIQUE) {
                        gboolean deleted = is_deleted(storage, event);

                        gint c = deleted ? NOSTRUM_STORAGE_ERROR_ALREADY_DELETED
                                         : NOSTRUM_STORAGE_ERROR_DUPLICATE;
                        set_sqlite_error (storage->db,
                                          "sqlite3_step(save event)",
                                          rc,
                                          c,
                                          err);

                } else {
                        set_sqlite_error (storage->db,
                                          "sqlite3_step(save event)",
                                          rc,
                                          NOSTRUM_STORAGE_ERROR_SAVE,
                                          err);
                }
                goto error_sql;
        }

        int changes = sqlite3_changes(storage->db);

        sqlite3_finalize (stmt);
        stmt = NULL;

        // did not inserted or updated any row
        if (changes == 0) { 
                set_sqlite_error (storage->db,
                                  "sqlite3_step(save event)",
                                  rc, // FIXME 101 its not sql error
                                  NOSTRUM_STORAGE_ERROR_NEWER_EXISTS,
                                  err);
                goto error_sql;
        }

        // GET row id ----------------------------------------------------------
        gint64 event_fk = -1;

        const char *sql_get_id = "SELECT id FROM events WHERE event_id = ?;";
        rc = sqlite3_prepare_v2 (storage->db, sql_get_id, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                set_sqlite_error(storage->db,
                                 "sqlite3_prepare_v2(select event id)",
                                 rc,
                                 NOSTRUM_STORAGE_ERROR_SAVE,
                                 err);
                goto error_sql;
        }

        sqlite3_bind_text (stmt, 1, event_id, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step (stmt);
        if (rc == SQLITE_ROW) {
                event_fk = sqlite3_column_int64 (stmt, 0);
        } else {
                set_sqlite_error(storage->db,
                                 "sqlite3_step(select event id)",
                                 rc,
                                 NOSTRUM_STORAGE_ERROR_SAVE,
                                 err);
                goto error_sql;
        }
        sqlite3_finalize (stmt);
        stmt = NULL;

        nostrum_event_set_storage_id(event, event_fk);

        // DELETE old tags -----------------------------------------------------
        const char *sql_del_tags = "DELETE FROM event_tags WHERE event_fk = ?;";
        rc = sqlite3_prepare_v2 (storage->db, sql_del_tags, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                set_sqlite_error(storage->db,
                                "sqlite3_prepare_v2(delete old tags)",
                                rc,
                                NOSTRUM_STORAGE_ERROR_SAVE,
                                err);
                goto error_sql;
        }

        sqlite3_bind_int64 (stmt, 1, event_fk);

        rc = sqlite3_step (stmt);
        if (rc != SQLITE_DONE) {
                set_sqlite_error(storage->db,
                                "sqlite3_step(delete old tags)",
                                rc,
                                NOSTRUM_STORAGE_ERROR_SAVE,
                                err);
                goto error_sql;
        }
        sqlite3_finalize (stmt);
        stmt = NULL;
        

        // INSERT TAGS ---------------------------------------------------------
        // TAGS ex: [ ["p", "value"], ["p", "value"], ["e", "value", "v"] ]
        const GPtrArray *tags = nostrum_event_get_tags (event);
        if (tags && tags->len > 0) {
                const char *sql_ins_tag = "INSERT OR IGNORE INTO event_tags"
                                          "(event_fk, tag, value, pos) "
                                          "VALUES(?, ?, ?, ?);";

                rc = sqlite3_prepare_v2 (storage->db, sql_ins_tag, -1,
                                         &stmt, NULL);
                if (rc != SQLITE_OK) {
                        set_sqlite_error(storage->db,
                                         "sqlite3_prepare_v2(insert tag)",
                                         rc,
                                         NOSTRUM_STORAGE_ERROR_SAVE,
                                         err);
                        goto error_sql;
                }

                for (guint pos = 0; pos < tags->len; pos++) {
                        const GPtrArray *tag_arr =
                            g_ptr_array_index ((GPtrArray *)tags, pos);
                        if (!tag_arr || tag_arr->len < 2)
                                continue;

                        // save only first two elements of tag array
                        const char *tag_name = g_ptr_array_index (tag_arr, 0);
                        const char *value = g_ptr_array_index (tag_arr, 1);

                        if (!tag_name || !value)
                                continue;

                        sqlite3_reset (stmt);
                        sqlite3_clear_bindings (stmt);

                        sqlite3_bind_int64 (stmt, 1, event_fk);
                        sqlite3_bind_text (stmt, 2, tag_name, -1,
                                           SQLITE_TRANSIENT);
                        sqlite3_bind_text (stmt, 3, value, -1,
                                           SQLITE_TRANSIENT);
                        sqlite3_bind_int (stmt, 4, (int)pos);

                        rc = sqlite3_step (stmt);
                        if (rc != SQLITE_DONE) {
                                set_sqlite_error(storage->db,
                                                 "sqlite3_step(insert tag)",
                                                 rc,
                                                 NOSTRUM_STORAGE_ERROR_SAVE,
                                                 err);
                                goto error_sql;
                        }
                }

                sqlite3_finalize (stmt);
                stmt = NULL;
        }

        if (!exec_sql_or_set_error (storage->db,
                                    "COMMIT;",
                                    NOSTRUM_STORAGE_ERROR_SAVE,
                                    err))
                goto error_sql;

        return TRUE;

error_sql:
        if (stmt)
                sqlite3_finalize (stmt);
        GError *tmp_err = NULL;
        exec_sql_or_set_error (storage->db, "ROLLBACK;", 0, &tmp_err);
        if (tmp_err) {
                g_warning ("Error rolling back transaction: %s",
                        tmp_err->message ? tmp_err->message : "(no message)");
        }
        g_clear_error (&tmp_err);
error:
        return FALSE;
}


guint
nostrum_storage_delete (const NostrumStorage *storage,
                        const GPtrArray      *events,
                        GError              **err)
{
        // Preconditions
        g_return_val_if_fail(storage != NULL, 0);
        g_return_val_if_fail(events != NULL, 0);
        g_return_val_if_fail(err == NULL || *err == NULL, 0);        

        if (events->len == 0) {
                return 0;
        }

        guint valid_evts = 0;
        sqlite3_stmt *stmt = NULL;
        g_autoptr(GString) sql = g_string_new("UPDATE events SET is_deleted = 1"
                                              " WHERE is_deleted = 0 AND"
                                              " id IN (");


        // BUILD SQL STRING WITH PALACEHOLDERS '?'------------------------------
        for (guint i = 0; i < events->len; i++) {
                NostrumEvent *evt = g_ptr_array_index((GPtrArray *)events, i);
                gint64 storage_id = nostrum_event_get_storage_id(evt);

                if (storage_id > 0) {
                        g_string_append_printf(sql, "%s?", valid_evts > 0 ? ","
                                                                          : "");
                        valid_evts++;
                }
        }
        g_string_append(sql, ");");

        if (valid_evts == 0) {
                return 0;
        }

        if (!exec_sql_or_set_error (storage->db,
                                    "BEGIN;",
                                    NOSTRUM_STORAGE_ERROR_DELETE,
                                    err))
                goto error_sql;

        int rc = sqlite3_prepare_v2(storage->db, sql->str, -1, &stmt, NULL);

        if (rc != SQLITE_OK) {
                set_sqlite_error(storage->db,
                                 "sqlite3_prepare_v2(delete/update event)",
                                 rc,
                                 NOSTRUM_STORAGE_ERROR_DELETE,
                                 err);
                goto error_sql;
        }

        // Bind params in the same order used to build SQL ---------------------
        int bind_index = 1;
        for (guint i = 0; i < events->len; i++) {
                NostrumEvent *evt = g_ptr_array_index((GPtrArray *)events, i);
                gint64 storage_id = nostrum_event_get_storage_id(evt);

                if (storage_id > 0) {
                        sqlite3_bind_int64(stmt, bind_index++, storage_id);
                }
        }

        rc = sqlite3_step(stmt);

        if (rc != SQLITE_DONE) {
                set_sqlite_error (storage->db,
                                  "sqlite3_step(delete/update event)",
                                  rc,
                                  NOSTRUM_STORAGE_ERROR_DELETE,
                                  err);
        }

        gint changes = sqlite3_changes(storage->db);

        sqlite3_finalize(stmt);
        stmt = NULL;

        if (!exec_sql_or_set_error (storage->db,
                                    "COMMIT;",
                                    NOSTRUM_STORAGE_ERROR_DELETE,
                                    err))
                goto error_sql;

        return changes;
error_sql:
        if (stmt)
                sqlite3_finalize (stmt);
        GError *tmp_err = NULL;
        exec_sql_or_set_error (storage->db, "ROLLBACK;", 0, &tmp_err);
        if (tmp_err) {
                g_warning ("Error rolling back transaction: %s",
                        tmp_err->message ? tmp_err->message : "(no message)");
        }
        g_clear_error (&tmp_err);
error:
        return 0;
}


/** 
 * Check if there is at least one non-empty filter.
 * We need to support empty filters, but if there is at least one
 * non-empty filter, we should ignore empty ones
*/
static gboolean
has_non_empty_filter(const GPtrArray *filters) {
        for (guint fi = 0; fi < filters->len; fi++) {
                const NostrumFilter *f = g_ptr_array_index (filters, fi);
                if (f && !nostrum_filter_is_empty(f)) {
                        return TRUE;
                }
        }
        return FALSE;
}


GPtrArray *
nostrum_storage_search (const NostrumStorage   *storage,
                        const GPtrArray        *filters,
                        GError                **err)
{
        // Preconditions (Must be true)
        g_return_val_if_fail (storage     != NULL,         NULL);
        g_return_val_if_fail (storage->db != NULL,         NULL);
        g_return_val_if_fail (filters     != NULL,         NULL);
        g_return_val_if_fail (err == NULL || *err == NULL, NULL);

        GPtrArray *events = 
            g_ptr_array_new_with_free_func ((GDestroyNotify)nostrum_event_free);

        if (filters->len == 0)
                return events;

        // Convert each filter to a GPtrArray of TagClause*
        // So it is a GPtrArray of GPtrArray of TagClause*
        g_autoptr (GPtrArray) per_filter_tag_clauses =
            get_tag_clauses_per_filter (filters);


        // BUILD SQL STRING WITH PALACEHOLDERS '?'------------------------------
        g_autoptr (GString) sql = g_string_new ("");

        gboolean first_query = TRUE;
        gboolean has_non_empty_f = has_non_empty_filter(filters);

        for (guint fi = 0; fi < filters->len; fi++) {
                const NostrumFilter *f = g_ptr_array_index (filters, fi);
                if (!f) {
                        continue;
                }

                // Adding a empty filter in this case would make the whole query
                // return all eventss
                if (has_non_empty_f && nostrum_filter_is_empty(f)) {
                        continue;
                }

                const GPtrArray *ids     = nostrum_filter_get_ids (f);
                const GPtrArray *authors = nostrum_filter_get_authors (f);
                const GArray    *kinds   = nostrum_filter_get_kinds (f);
                gint64           since   = nostrum_filter_get_since (f);
                gint64           until   = nostrum_filter_get_until (f);
                gint             limit   = nostrum_filter_get_limit (f);

                const GPtrArray *dedup_keys = nostrum_filter_get_dedup_keys (f);

                if (limit == 0) {
                        continue;
                }

                if (!first_query)
                        g_string_append (sql, " UNION ");
                first_query = FALSE;

                g_string_append (sql,
                    "SELECT e.raw_json, e.created_at, e.id, e.dedup_key "
                    "FROM events e WHERE (e.is_deleted = 0");

                if (ids && ids->len > 0) {
                        g_string_append (sql, " AND e.event_id IN ");
                        append_in_list_placeholders (sql, ids->len);
                }

                if (authors && authors->len > 0) {
                        g_string_append (sql, " AND e.pubkey IN ");
                        append_in_list_placeholders (sql, authors->len);
                }

                if (dedup_keys && dedup_keys->len > 0) {
                        g_string_append (sql, " AND e.dedup_key IN ");
                        append_in_list_placeholders (sql, dedup_keys->len);
                }

                if (kinds && kinds->len > 0) {
                        g_string_append (sql, " AND e.kind IN ");
                        append_in_list_placeholders (sql, kinds->len);
                }

                if (since >= 0)
                        g_string_append (sql, " AND e.created_at >= ?");

                if (until >= 0)
                        g_string_append (sql, " AND e.created_at <= ?");

                // Tags
                GPtrArray *clauses = g_ptr_array_index (per_filter_tag_clauses,
                                                        fi);
                if (clauses && clauses->len > 0) {
                        for (guint ci = 0; ci < clauses->len; ci++) {
                                TagClause *c = g_ptr_array_index (clauses, ci);
                                if (!c || !c->key || !c->vals
                                    || c->vals->len == 0)
                                        continue;

                                append_tag_exists_clause (sql, c->vals->len);
                        }
                }

                //g_string_append (sql, ") ORDER BY e.created_at DESC");
                g_string_append (sql, ")");

                if (limit > -1)
                        g_string_append_printf (sql, " LIMIT %d", limit);
        }

        if (first_query)
                return events;

        g_string_prepend (sql, "SELECT raw_json, id, dedup_key FROM (");
        g_string_append (sql, ") ORDER BY created_at DESC;");

        g_debug ("Executing SQL: %s", sql->str);

        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2 (storage->db, sql->str, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                set_sqlite_error(storage->db,
                                 "sqlite3_prepare_v2(search events)",
                                 rc,
                                 NOSTRUM_STORAGE_ERROR_SEARCH,
                                 err);
                goto error_sql;
        }


        int bind_i = 1;

        // Bind params in the same order used to build SQL ---------------------
        for (guint fi = 0; fi < filters->len; fi++) {
                const NostrumFilter *f = g_ptr_array_index (filters, fi);
                if (f == NULL)
                        continue;

                const GPtrArray *ids     = nostrum_filter_get_ids (f);
                const GPtrArray *authors = nostrum_filter_get_authors (f);
                const GArray    *kinds   = nostrum_filter_get_kinds (f);
                gint64           since   = nostrum_filter_get_since (f);
                gint64           until   = nostrum_filter_get_until (f);
                gint             limit   = nostrum_filter_get_limit (f);

                const GPtrArray *dedup_keys = nostrum_filter_get_dedup_keys (f);

                if (limit == 0) {
                        continue;
                }

                if (ids) {
                        for (guint i = 0; i < ids->len; i++) {
                                const char *v = g_ptr_array_index (ids, i);
                                g_debug ("Binding id param (%d): %s", bind_i,
                                         v);
                                sqlite3_bind_text (stmt, bind_i++, v, -1,
                                                   SQLITE_TRANSIENT);
                        }
                }

                if (authors) {
                        for (guint i = 0; i < authors->len; i++) {
                                const char *v = g_ptr_array_index (authors, i);
                                g_debug ("Binding author param (%d): %s",
                                         bind_i, v);
                                sqlite3_bind_text (stmt, bind_i++, v, -1,
                                                   SQLITE_TRANSIENT);
                        }
                }

                if (dedup_keys) {
                        for (guint i = 0; i < dedup_keys->len; i++) {
                                const char *v = g_ptr_array_index (dedup_keys,
                                                                   i);
                                g_debug ("Binding dedup_key param (%d): %s",
                                         bind_i, v);
                                sqlite3_bind_text (stmt, bind_i++, v, -1,
                                                   SQLITE_TRANSIENT);
                        }
                }

                if (kinds) {
                        for (guint i = 0; i < kinds->len; i++) {
                                gint v = g_array_index (kinds, gint, i);
                                g_debug ("Binding kind param (%d): %d",
                                         bind_i, v);
                                sqlite3_bind_int (stmt, bind_i++, v);
                        }
                }

                if (since >= 0) {
                        g_debug ("Binding since param (%d): %" G_GINT64_FORMAT,
                                 bind_i, since);
                        sqlite3_bind_int64 (stmt, bind_i++, since);
                }

                if (until >= 0) {
                        g_debug ("Binding until param (%d): %" G_GINT64_FORMAT,
                                 bind_i, until);
                        sqlite3_bind_int64 (stmt, bind_i++, until);
                }

                // Binding tags ------------------------------------------------
                GPtrArray *clauses = g_ptr_array_index (per_filter_tag_clauses,
                                                        fi);
                if (clauses && clauses->len > 0) {
                        for (guint ci = 0; ci < clauses->len; ci++) {
                                TagClause *c = g_ptr_array_index (clauses, ci);
                                if (!c || !c->key || !c->vals
                                    || c->vals->len == 0)
                                        continue;

                                // et.tag = ?
                                // Remove '#'
                                char *key = c->key;
                                if (key[0] == '#') {
                                        key++;
                                }
                                g_debug ("Binding tag key param (%d): %s",
                                         bind_i, key);
                                sqlite3_bind_text (stmt, bind_i++, key, -1,
                                                   SQLITE_TRANSIENT);

                                // et.value IN (...)
                                g_debug ("Binding tag value params (%d-%d):",
                                         bind_i, bind_i + c->vals->len - 1);
                                for (guint vi = 0; vi < c->vals->len; vi++) {
                                        const char *val =
                                            g_ptr_array_index (c->vals, vi);
                                        g_debug (" Binding %s", val);
                                        sqlite3_bind_text (stmt, bind_i++,
                                                           val, -1,
                                                           SQLITE_TRANSIENT);
                                }
                        }
                }
        }

        // Iterate over results ------------------------------------------------
        while ((rc = sqlite3_step (stmt)) == SQLITE_ROW) {
                const unsigned char *raw = sqlite3_column_text (stmt, 0);
                if (!raw)
                        continue;

                sqlite3_int64 storage_id = sqlite3_column_int64 (stmt, 1);
                const unsigned char *dedup_key = sqlite3_column_text (stmt, 2);


                g_autoptr (GError) parse_err = NULL;
                NostrumEvent *ev =
                    nostrum_event_from_json ((const char *)raw, &parse_err);
                if (!ev) {
                        // FIXME
                        g_warning ("nostrum_storage_search: cannot parse stored"
                                   " raw_json: %s",
                                   parse_err ? parse_err->message
                                             : "(no error)");
                        continue;
                }

                if (dedup_key) {
                        nostrum_event_set_dedup_key (ev,
                                                     (const char *)dedup_key);
                }
                nostrum_event_set_storage_id (ev, storage_id);
                // events owns ev, frees via nostrum_event_free
                g_ptr_array_add (events, ev);
        }

        if (rc != SQLITE_DONE) {
                set_sqlite_error (storage->db,
                                  "sqlite3_step(search events)",
                                  rc,
                                  NOSTRUM_STORAGE_ERROR_SEARCH,
                                  err);
                goto error_sql;

        }

        sqlite3_finalize (stmt);        
        return events;

error_sql:
        if (stmt)
                sqlite3_finalize (stmt);
        return NULL;
}


// =============================================================================
// HELPERS - IMPLEMENTATION
// =============================================================================


// Returns "(?, ?, ?)" etc
static void
append_in_list_placeholders (GString *s, guint n)
{
        // Generates "(?, ?, ?)" etc
        g_string_append_c (s, '(');
        for (guint i = 0; i < n; i++) {
                if (i)
                        g_string_append (s, ",");
                g_string_append (s, "?");
        }
        g_string_append_c (s, ')');
}


static void
set_sqlite_error (sqlite3       *db,
                  const char    *where,
                  int            rc,

                  gint           code,
                  GError       **err)
{
        g_return_if_fail (where != NULL);
        g_return_if_fail (err == NULL || *err == NULL);

        g_set_error (err,
                     NOSTRUM_STORAGE_ERROR,
                     code,
                     "SQLite error at %s: rc=%d: %s",
                     where,
                     rc,
                     db ? sqlite3_errmsg (db) : "no db");
}

static gboolean
exec_sql_or_set_error (sqlite3     *db,
                       const char  *sql,
                       gint         code,
                       GError     **err)
{
        // Preconditions
        g_return_val_if_fail (db != NULL, FALSE);
        g_return_val_if_fail (sql != NULL, FALSE);
        g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

        char *errmsg = NULL;
        int rc = sqlite3_exec (db, sql, NULL, NULL, &errmsg);

        if (rc != SQLITE_OK) {
                const char *dbmsg = sqlite3_errmsg (db);

                g_set_error (err,
                             NOSTRUM_STORAGE_ERROR,
                             code,
                             "SQLite error: rc=%d, errmsg=%s, dbmsg=%s, sql=%s",
                             rc,
                             errmsg ? errmsg : "(no msg)",
                             dbmsg ? dbmsg : "(no dbmsg)",
                             sql);

                sqlite3_free (errmsg);
                return FALSE;
        }

        sqlite3_free (errmsg);
        return TRUE;
}


static gint
cmp_str (gconstpointer a, gconstpointer b)
{
        const char *sa = *(const char *const *)a;
        const char *sb = *(const char *const *)b;
        return g_strcmp0 (sa, sb);
}

// Returns GPtrArray of TagClause*, sorted by tag key
static GPtrArray *
filter_build_sorted_tag_clauses (const NostrumFilter *f)
{
        const GHashTable *ht = nostrum_filter_get_tags (f);

        if (!ht || g_hash_table_size ((GHashTable *)ht) == 0)
                return g_ptr_array_new_with_free_func (g_free);

        // Get all keys --------------------------------------------------------
        g_autoptr (GPtrArray) keys = g_ptr_array_new ();
        GHashTableIter iter;
        gpointer k = NULL;
        gpointer v = NULL;

        g_hash_table_iter_init (&iter, (GHashTable *)ht);
        while (g_hash_table_iter_next (&iter, &k, &v)) {
                const char      *tag_key = (const char *)k;
                const GPtrArray *vals    = (const GPtrArray *)v;
                if (!tag_key || !vals || vals->len == 0)
                        continue;
                g_ptr_array_add (keys, (gpointer)tag_key); // borrowed
        }

        // Sort keys -----------------------------------------------------------
        g_ptr_array_sort (keys, cmp_str);

        // Build TagClauses in sorted key order --------------------------------
        GPtrArray *clauses = g_ptr_array_new_with_free_func (g_free);
        for (guint i = 0; i < keys->len; i++) {
                const char *tag_key = g_ptr_array_index (keys, i);
                const GPtrArray *vals = g_hash_table_lookup ((GHashTable *)ht,
                                                             tag_key);
                if (!tag_key || !vals || vals->len == 0)
                        continue;

                TagClause *c = g_new0 (TagClause, 1);
                c->key = tag_key; // borrowed
                c->vals = vals;   // borrowed
                g_ptr_array_add (clauses, c);
        }

        // owned, elements TagClause freed by free_func, but not key/vals
        return clauses;
}

// Returns GPtrArray of GPtrArray of TagClause*
static 
GPtrArray *
get_tag_clauses_per_filter (const GPtrArray *filters)
{
        g_autoptr (GPtrArray) per_filter_tag_clauses =
            g_ptr_array_new_with_free_func (
                (GDestroyNotify)g_ptr_array_unref);
        
        for (guint fi = 0; fi < filters->len; fi++) {
                const NostrumFilter *f =
                    g_ptr_array_index ((GPtrArray *)filters, fi);
                if (f == NULL) {
                        g_ptr_array_add (per_filter_tag_clauses, NULL);
                        continue;
                }
                GPtrArray *clauses = filter_build_sorted_tag_clauses (f);
                g_ptr_array_add (per_filter_tag_clauses, clauses);
        }
        return g_steal_pointer (&per_filter_tag_clauses);
}

static void
append_tag_exists_clause (GString *sql, guint n_values)
{
        g_string_append (sql, " AND EXISTS (SELECT 1 FROM event_tags et");
        g_string_append (sql, " WHERE et.event_fk = e.id AND et.tag = ? "
                              "AND et.value IN ");
        append_in_list_placeholders (sql, n_values);
        g_string_append (sql, ")");
}
