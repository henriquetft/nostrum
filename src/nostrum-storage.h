/* ========================================================================== */
/* Copyright (c) 2026 Henrique Teófilo                                        */
/* All rights reserved.                                                       */
/*                                                                            */
/* Definition of NostrumStorage (storage of events).                          */
/*                                                                            */
/* This file is part of Nostrum Project.                                      */
/* For the terms of usage and distribution, please see COPYING file.          */
/* ========================================================================== */

#ifndef NOSTRUM_STORAGE_H
#define NOSTRUM_STORAGE_H

#include "nostrum-event.h"
#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
        NOSTRUM_STORAGE_ERROR_INIT,
        NOSTRUM_STORAGE_ERROR_SAVE,
        NOSTRUM_STORAGE_ERROR_SEARCH,
        NOSTRUM_STORAGE_ERROR_DUPLICATE,
        NOSTRUM_STORAGE_ERROR_NEWER_EXISTS,
        NOSTRUM_STORAGE_ERROR_DELETE,
        NOSTRUM_STORAGE_ERROR_ALREADY_DELETED
} NostrumStorageError;


GQuark nostrum_storage_error_quark (void);
#define NOSTRUM_STORAGE_ERROR (nostrum_storage_error_quark ())

typedef struct _NostrumStorage NostrumStorage;

// CONSTRUCTORS / DESTRUCTORS --------------------------------------------------
NostrumStorage *nostrum_storage_new     (const gchar *db_file);

void            nostrum_storage_free    (NostrumStorage *storage);

// OPERATIONS ------------------------------------------------------------------
gboolean        nostrum_storage_init    (NostrumStorage *storage, GError **err);


gboolean        nostrum_storage_save    (NostrumStorage          *storage,
                                         NostrumEvent            *event,
                                         GError                 **err);

GPtrArray *     nostrum_storage_search  (const NostrumStorage    *storage,
                                         const GPtrArray         *filters,
                                         GError                  **err);

guint           nostrum_storage_delete  (const NostrumStorage    *storage,
                                         const GPtrArray         *events,
                                         GError                  **err);

// -----------------------------------------------------------------------------

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NostrumStorage, nostrum_storage_free)

G_END_DECLS

#endif // NOSTRUM_STORAGE_H