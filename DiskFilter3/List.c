#include "List.h"

#if defined(USE_LRU) || defined(USE_SLRU) || defined(USE_OCP)

VOID InitList (PList List)
{
    List->Size = 0;
    List->Head = NULL;
    List->Tail = NULL;
}

VOID DestroyList (PList List)
{
    List->Size = 0;
    List->Head = NULL;
    List->Tail = NULL;
}

LIST_DAT_T* ListRemoveHead (PList List)
{
    LIST_DAT_T* ret;
    if (List->Size == 0)
        return NULL;
    ret = List->Head;
    if (List->Size == 1)
        List->Head = List->Tail = NULL;
    else
    {
        List->Head = List->Head->Next;
        List->Head->Prior = NULL;
    }
    List->Size--;
    return ret;
}

LIST_DAT_T* ListRemoveTail (PList List)
{
    LIST_DAT_T* ret;
    if (List->Size == 0)
        return NULL;
    ret = List->Tail;
    if (List->Size == 1)
        List->Head = List->Tail = NULL;
    else
    {
        List->Tail = List->Tail->Prior;
        List->Tail->Next = NULL;
    }
    List->Size--;
    return ret;
}

VOID ListInsertToHead (PList List, LIST_DAT_T* Entry)
{
    if (List->Head != NULL)
    {
        Entry->Prior = NULL;
        Entry->Next = List->Head;
        List->Head->Prior = Entry;
        List->Head = Entry;
    }
    else
    {
        List->Head = List->Tail = Entry;
        Entry->Next = Entry->Prior = NULL;
    }
    List->Size++;
}

VOID ListInsertToTail (PList List, LIST_DAT_T* Entry)
{
    if (List->Tail != NULL)
    {
        List->Tail->Next = Entry;
        Entry->Prior = List->Tail;
        Entry->Next = NULL;
        List->Tail = Entry;
    }
    else
    {
        List->Head = List->Tail = Entry;
        Entry->Next = Entry->Prior = NULL;
    }
    List->Size++;
}

VOID ListInsertAfter (PList List, LIST_DAT_T* After, LIST_DAT_T* Entry)
{
    if (After == List->Tail)
        ListInsertToTail(List, Entry);
    else
    {
        Entry->Prior = After;
        Entry->Next = After->Next;
        After->Next = Entry;
        Entry->Next->Prior = Entry;
        List->Size++;
    }
}

VOID ListInsertBefore (PList List, LIST_DAT_T* Before, LIST_DAT_T* Entry)
{
    LIST_DAT_T* After = Before->Prior;
    if (After == NULL)
        ListInsertToHead(List, Entry);
    else
        ListInsertAfter(List, After, Entry);
}

VOID ListDelete (PList List, LIST_DAT_T* Entry)
{
    if (Entry == List->Head)
        ListRemoveHead(List);
    else if (Entry == List->Tail)
        ListRemoveTail(List);
    else
    {
        Entry->Prior->Next = Entry->Next;
        Entry->Next->Prior = Entry->Prior;
        List->Size--;
    }
}

VOID ListMoveToHead (PList List, LIST_DAT_T* Entry)
{
    if (Entry == List->Head)
        return;
    ListDelete(List, Entry);
    ListInsertToHead(List, Entry);
}

VOID ListMoveToTail (PList List, LIST_DAT_T* Entry)
{
    if (Entry == List->Tail)
        return;
    ListDelete(List, Entry);
    ListInsertToTail(List, Entry);
}

#endif
