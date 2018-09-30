#include <stdio.h>
#include <stdlib.h>
#include "my402list.h"

int  My402ListLength(My402List* list){
    return list->num_members;
}

int  My402ListEmpty(My402List* list){
    if(list->num_members <= 0)
        return TRUE;
    else
        return FALSE;
}

int My402ListAppend(My402List* list, void* newobj){
    My402ListElem * newnode = (My402ListElem *) malloc(sizeof(My402ListElem));  
    if(newnode==NULL)
        return FALSE;
    newnode->obj = newobj;
    My402ListElem * lastnode = My402ListLast(list);
    if(My402ListEmpty(list)){
        list->anchor.prev = newnode;
        list->anchor.next = newnode;
        newnode->prev = &(list->anchor);
        newnode->next = &(list->anchor);
        list->num_members++;
        return TRUE;
    }
    newnode->prev = lastnode;
    newnode->next = &(list->anchor);
    lastnode->next = newnode;
    list->anchor.prev = newnode;
    list->num_members++;
    return TRUE;
}

int  My402ListPrepend(My402List* list, void* newobj){
    My402ListElem * newnode = (My402ListElem *) malloc(sizeof(My402ListElem));
    if(newnode==NULL)
        return FALSE;
    newnode->obj = newobj;
    My402ListElem * firstnode = My402ListFirst(list);
    if(My402ListEmpty(list)){
        list->anchor.prev = newnode;
        list->anchor.next = newnode;
        newnode->prev = &(list->anchor);
        newnode->next = &(list->anchor);
        list->num_members++;
        return TRUE;
    }
    newnode->prev = &(list->anchor);
    newnode->next = firstnode;
    firstnode->prev = newnode;
    list->anchor.next = newnode;
    list->num_members++;
    return TRUE;
}

void My402ListUnlink(My402List* list, My402ListElem* elem){
    if(!My402ListEmpty(list) || elem)
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
        free(elem);
        list->num_members--;
}

void My402ListUnlinkAll(My402List* list){
    My402ListElem * cur = My402ListFirst(list);
    if(cur == NULL){
        return;
    }
    My402ListElem * curnext = cur->next;
    while (cur!=&(list->anchor)){
        My402ListUnlink(list, cur);
        cur = curnext;
        curnext = cur->next;
    }
}

int  My402ListInsertAfter(My402List* list, void* newobj, My402ListElem* elem){
    if(elem==NULL){
        return My402ListAppend(list, newobj);
    }
    My402ListElem * newnode = (My402ListElem *) malloc(sizeof(My402ListElem));
    if(newnode==NULL)
        return FALSE;
    newnode->obj = newobj;
    newnode->prev = elem;
    newnode->next = elem->next;
    elem->next->prev = newnode;
    elem->next = newnode;
    list->num_members++;
    return TRUE;
}

int  My402ListInsertBefore(My402List* list, void* newobj, My402ListElem* elem){
    if(elem==NULL){
        return My402ListPrepend(list, newobj);
    }
    My402ListElem * newnode = (My402ListElem *) malloc(sizeof(My402ListElem));
    if(newnode==NULL)
        return FALSE;
    newnode->obj = newobj;
    newnode->next = elem;
    newnode->prev = elem->prev;
    elem->prev->next = newnode;
    elem->prev = newnode;
    list->num_members++;
    return TRUE;
}

My402ListElem *My402ListFirst(My402List* list){
    if(list->num_members==0)
        return NULL;
    return list->anchor.next;
}

My402ListElem *My402ListLast(My402List* list){
    if(list->num_members==0)
        return NULL;
    return list->anchor.prev;
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){
    if(list->num_members==0 || elem->next==&(list->anchor))
        return NULL;
    return elem->next;
}   

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){
    if(list->num_members==0 || elem->prev==&(list->anchor))
        return NULL;
    return elem->prev;
}

My402ListElem *My402ListFind(My402List* list, void* object){
    My402ListElem * cur = My402ListFirst(list);
    while (cur!=&(list->anchor)) {
        if(cur->obj==object)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

int My402ListInit(My402List* list){
    if(list!=NULL)
        list->anchor.prev = &(list->anchor);
        list->anchor.next = &(list->anchor);
        list->num_members = 0;
        return TRUE;
    return FALSE;
}
