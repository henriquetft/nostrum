/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Miscellaneous utility functions. (Impl. of nostrum-utils.h)                */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#include "nostrum-utils.h"

GPtrArray *
nostrum_utils_new_str_ptrarray (const char **vals)
{
        GPtrArray *arr = g_ptr_array_new_with_free_func (g_free);
        for (guint i = 0; vals[i]; i++) {
                g_ptr_array_add (arr, g_strdup (vals[i]));
        }
        return arr;
}

GArray *
nostrum_utils_new_int_garray (const int *vals, guint n)
{
        GArray *ga = g_array_sized_new (FALSE, FALSE, sizeof (gint), n);
        for (guint i = 0; i < n; i++) {
                gint v = vals[i];
                g_array_append_val (ga, v);
        }
        return ga;
}

GHashTable *
nostrum_utils_new_table_str_to_ptrarray (const char *key, const char **values)
{
        GHashTable *ht =
            g_hash_table_new_full (g_str_hash,
                                   g_str_equal,
                                   g_free,
                                   (GDestroyNotify)g_ptr_array_unref);
        g_hash_table_insert (ht,
                             g_strdup (key),
                             nostrum_utils_new_str_ptrarray (values));
        return ht;
}



char *
nostrum_utils_sha256_hex_lower (const char *utf8)
{
        g_return_val_if_fail (utf8 != NULL, NULL);

        g_autoptr (GChecksum) chk = g_checksum_new (G_CHECKSUM_SHA256);
        g_checksum_update (chk, (const guchar *)utf8, (gssize)strlen (utf8));

        return g_strdup (g_checksum_get_string (chk));
}


gboolean
nostrum_utils_hex_to_bytes (const char *hex, guint8 *out, gsize out_len)
{
        gsize n = strlen (hex);
        if (n != out_len * 2)
                return FALSE;
        for (gsize i = 0; i < out_len; i++) {
                char b[3] = { hex[2 * i], hex[2 * i + 1], 0 };
                char *end = NULL;
                long v = strtol (b, &end, 16);
                if (!end || *end)
                        return FALSE;
                out[i] = (guint8)v;
        }
        return TRUE;
}

gboolean
nostrum_utils_sha256_to_bytes (const char *data, gssize len, guint8 out32[32])
{
        if (len < 0)
                len = (gssize)strlen (data);

        GChecksum *chk = g_checksum_new (G_CHECKSUM_SHA256);
        if (!chk)
                return FALSE;

        g_checksum_update (chk, (const guchar *)data, len);

        gsize got = 32;
        g_checksum_get_digest (chk, out32, &got);
        g_checksum_free (chk);

        return (got == 32) ? TRUE : FALSE;
}

gboolean
nostrum_utils_is_hex_len (const char *s, gsize expect_len)
{
        if (!s)
                return FALSE;
        if (strlen (s) != expect_len)
                return FALSE;
        for (const unsigned char *p = (const unsigned char *)s; *p; p++)
                if (!g_ascii_isxdigit (*p))
                        return FALSE;
        return TRUE;
}


GPtrArray *
nostrum_utils_dup_str_ptr_array (const GPtrArray *src)
{
        if (!src)
                return NULL;

        GPtrArray *dst = g_ptr_array_new_with_free_func (g_free);

        for (guint i = 0; i < src->len; i++) {
                const gchar *s = g_ptr_array_index ((GPtrArray *) src, i);
                if (!s) {
                        g_ptr_array_add (dst, NULL);
                        continue;
                }
                g_ptr_array_add (dst, g_strdup (s));
        }

        return dst;
}

GArray *
nostrum_utils_dup_int_garray (const GArray *src)
{
        if (!src)
                return NULL;

        GArray *dst = g_array_sized_new (FALSE, FALSE, sizeof (gint), src->len);

        for (guint i = 0; i < src->len; i++) {
                gint v = g_array_index ((GArray *) src, gint, i);
                g_array_append_val (dst, v);
        }

        return dst;
}
