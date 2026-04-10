/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Miscellaneous utility functions.                                           */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include <glib.h>

G_BEGIN_DECLS

GPtrArray *
nostrum_utils_new_str_ptrarray (const char **vals);

GArray *
nostrum_utils_new_int_garray   (const int *vals, guint n);

GHashTable *
nostrum_utils_new_table_str_to_ptrarray (const char *key, const char **values);

char *
nostrum_utils_sha256_hex_lower (const char *utf8);

gboolean
nostrum_utils_is_hex_len       (const char *s, gsize expect_len);

gboolean
nostrum_utils_hex_to_bytes     (const char *hex, guint8 *out, gsize out_len);

gboolean
nostrum_utils_sha256_to_bytes  (const char *data, gssize len, guint8 out32[32]);

GPtrArray *
nostrum_utils_dup_str_ptr_array(const GPtrArray *src);

GArray *
nostrum_utils_dup_int_garray   (const GArray *src);

G_END_DECLS