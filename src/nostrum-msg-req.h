/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumMsgReq object.                                        */
/* A representation of Nostr REQ message                                      */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_MSG_REQ_H
#define NOSTRUM_MSG_REQ_H


#include <glib.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>
#include "nostrum-filter.h"


// NIP-01 message:
// ["REQ", "<subscription_id>", <filter1>, <filter2>, ...]

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_MSG_REQ_ERROR_PARSE,
} NostrumReqError;

GQuark nostrum_msg_req_error_quark (void);
#define NOSTRUM_MSG_REQ_ERROR (nostrum_msg_req_error_quark ())

typedef struct _NostrumMsgReq NostrumMsgReq;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------

/**
 * nostrum_msg_req_new:
 *
 * Creates a new empty #NostrumMsgReq.
 *
 * Returns: (transfer full): a newly allocated #NostrumMsgReq.
 */
NostrumMsgReq *nostrum_msg_req_new (void);

/**
 * nostrum_msg_req_free:
 * @req: (nullable): a #NostrumMsgReq
 *
 * Frees @req and all its owned contents.
 */
void        nostrum_msg_req_free (NostrumMsgReq *req);

// JSON CONVERSIONS ------------------------------------------------------------

NostrumMsgReq *
nostrum_msg_req_from_json_node (const JsonNode *root, GError **err);
/**
 * nostrum_msg_req_from_json:
 * @json: JSON string representing a REQ message
 * @err: (out) (optional): return location for a #GError, or %NULL
 *
 * Parses a REQ message with the format:
 *   ["REQ", <sub_id>, <filter1>, <filter2>, ...]
 *
 * Filters are converted into #NostrumFilter instances.
 *
 * Returns: (transfer full): a new #NostrumMsgReq or %NULL on error.
 */
NostrumMsgReq *
nostrum_msg_req_from_json (const char *json, GError **err);

/**
 * nostrum_msg_req_to_json:
 * @req: a #NostrumMsgReq
 *
 * Serializes @req into a JSON string in the REQ format.
 *
 * Returns: (transfer full): a newly allocated JSON string.
 */
gchar *
nostrum_msg_req_to_json (const NostrumMsgReq *req);

// OPERATIONS ------------------------------------------------------------------

/**
 * nostrum_msg_req_add_filter:
 * @req: a #NostrumMsgReq
 * @filter: (transfer full): a #NostrumFilter
 *
 * Appends @filter to the filters array, taking ownership of it.
 */
void
nostrum_msg_req_take_one_filter (NostrumMsgReq *req, NostrumFilter *filter);

// GETTERS ---------------------------------------------------------------------
/**
 * nostrum_msg_req_get_sub_id:
 * @req: a #NostrumMsgReq
 *
 * Gets the subscription id of this REQ message.
 *
 * Returns: (nullable) (transfer none): the subscription id.
 */
const char *
nostrum_msg_req_get_sub_id (const NostrumMsgReq *req);

/**
 * nostrum_msg_req_get_filters:
 * @req: a #NostrumMsgReq
 *
 * Gets the filters list of this REQ message.
 *
 * Returns: (transfer none): a #GPtrArray of #NostrumFilter*,
 *          or %NULL if there are no filters.
 */
const GPtrArray *
nostrum_msg_req_get_filters (const NostrumMsgReq *req);

/**
 * nostrum_msg_req_dup_filters:
 * @req: a #NostrumMsgReq
 *
 * Returns a deep copy of the REQ filters.
 *
 * Returns: (transfer full) (nullable): array of #NostrumFilter
 */
GPtrArray *
nostrum_msg_req_dup_filters (const NostrumMsgReq *req);

// SETTERS ---------------------------------------------------------------------

/**
 * nostrum_msg_req_set_sub_id:
 * @req: a #NostrumMsgReq
 * @sub_id: (nullable): subscription id
 *
 * Sets the subscription id, copying @sub_id.
 */
void
nostrum_msg_req_set_sub_id (NostrumMsgReq *req, const char *sub_id);

/**
 * nostrum_msg_req_take_filters:
 * @req: a #NostrumMsgReq
 * @filters: (transfer full) (nullable): a #GPtrArray of #NostrumFilter*
 *
 * Sets the filters array of @req, taking ownership of @filters.
 * The previous array (if any) is unreffed.
 *
 * The array is expected to be created with a suitable free func
 * (for example, #nostrum_filter_free).
 */
void
nostrum_msg_req_take_filters (NostrumMsgReq *req, GPtrArray *filters);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumMsgReq, nostrum_msg_req_free)

G_END_DECLS

#endif // NOSTRUM_MSG_REQ_H
