#pragma once

#include "Cache.h"

VOID InitList (PList List);

VOID DestroyList (PList List);

LIST_DAT_T* ListRemoveHead (PList List);

LIST_DAT_T* ListRemoveTail (PList List);

VOID ListInsertToHead (PList List, LIST_DAT_T* Entry);

VOID ListInsertToTail (PList List, LIST_DAT_T* Entry);

VOID ListInsertAfter (PList List, LIST_DAT_T* After, LIST_DAT_T* Entry);

VOID ListInsertBefore (PList List, LIST_DAT_T* Before, LIST_DAT_T* Entry);

VOID ListDelete (PList List, LIST_DAT_T* Entry);

VOID ListMoveToHead (PList List, LIST_DAT_T* Entry);

VOID ListMoveToTail (PList List, LIST_DAT_T* Entry);
