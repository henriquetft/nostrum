/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* JSON helper functions.                                                     */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef JSON_UTILS_H_
#define JSON_UTILS_H_

#include <glib.h>
#include <json-glib/json-glib.h>
#include "nostrum-filter.h"

G_BEGIN_DECLS

GPtrArray *nostrum_json_utils_parse_str_array (JsonNode *node);

// array of ints -> GArray* (element = gint)
GArray *nostrum_json_utils_parse_int_array (JsonNode *node);

GHashTable *
nostrum_json_utils_parse_tags (JsonObject *obj);

char *
nostrum_json_utils_node_to_str (JsonNode *node);

G_END_DECLS

#endif