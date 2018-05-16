#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "dll.h"

typedef struct node NODE;
struct node {
    NODE *next, *prev;
    void *value;
};
NODE *newNODE(void *v) {
    NODE *n = malloc(sizeof(NODE));
    assert(n != 0);

    n->value = v;
    n->next = NULL;
    n->prev = NULL;
    return n;
}

struct dll {
    NODE *head, *tail;
    int size;
    void (*display)(void *, FILE *);
    void (*free)(void *);
};

DLL *newDLL(void (*d)(void *,FILE *), void (*f)(void *)) {
    DLL *list = malloc(sizeof(DLL));
    assert(list != 0);

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->display = d;
    list->free = f;
    return list;
}
void insertDLL(DLL *items, int index, void *value) {
    assert(index >= 0 && index <= items->size);
    NODE *item = newNODE(value);
    if(items->size == 0) {
        items->head = item;
        items->tail = item;
        items->size++;
        item->next = item;
        item->prev = item;
    } else {
        NODE *rightNode, *leftNode;
        if(index <= items->size/2) {
            int i=0;
            rightNode = items->head;
            while(i < index) {
                rightNode = rightNode->next;
                i++;
            }
            leftNode = rightNode->prev;
        } else {
            int i=items->size-1;
            leftNode = items->tail;
            while(i > index) {
                leftNode = leftNode->prev;
                i--;
            }
            rightNode = leftNode->next;
        }
        leftNode->next = item;
        rightNode->prev = item;
        item->next = rightNode;
        item->prev = leftNode;
        items->size++;
        if(index == 0)
            items->head = item;
        if(index == items->size-1)
            items->tail = item;
    }
}
void *removeDLL(DLL *items, int index) {
    assert(items->size > 0);
    assert(index >= 0 && index < items->size);
    NODE *item, *left, *right;
    if(index <= items->size/2) {
        item = items->head;
        for(int i=0; i<index; i++) {
            item = item->next;
        }
    } else {
        item = items->tail;
        for(int i=items->size-1; i>index; i--) {
            item = item->prev;
        }
    }
    left = item->prev;
    right = item->next;
    left->next = right;
    right->prev = left;
    if(index == 0)
        items->head = right;
    if(index == items->size-1)
        items->tail = left;
    items->size--;
    void *temp = item->value;
    free(item);
    return temp;
}
void unionDLL(DLL *recipient, DLL *donor) {
    if(recipient->size != 0 && donor->size != 0) {
        recipient->tail->next = donor->head;
        donor->head->prev = recipient->tail;
        recipient->head->prev = donor->tail;
        donor->tail->next = recipient->head;
        recipient->size += donor->size;
        freeDLL(donor);
    } else if(recipient->size == 0 && donor->size != 0) {
        DLL *temp = recipient;
        recipient = donor;
        free(temp);
    } else {    //When donor list is empty
        free(donor);    //Nothing to be done except free donor list
    }
}
void *getDLL(DLL *items, int index) {
    assert(index >= 0 && index < items->size);
    NODE *item;
    if(index <= items->size/2) {
        item = items->head;
        for(int i=0; i<index; i++) {
            item = item->next;
        }
    } else {
        item = items->tail;
        for(int i=items->size-1; i>items->size; i--) {
            item = item->prev;
        }
    }
    return item->value;
}
void *setDLL(DLL *items, int index, void *value) {
    assert(index >= 0 && index < items->size);
    NODE *item;
    if(index <= items->size/2) {
        item = items->head;
        for(int i=0; i<index; i++) {
            item = item->next;
        }
    } else {
        item = items->tail;
        for(int i=items->size-1; i>items->size; i--) {
            item = item->prev;
        }
    }
    void *oldVal = item->value;
    item->value = value;
    return oldVal;
}
int findDLL(DLL *items, void *value) {
    NODE *item = items->head;
    for(int i=0; i<items->size; i++) {
        if(item->value == value)
            return i;
        item = item->next;
    }
    return -1;
}
int sizeDLL(DLL *items) {
    return items->size;
}
void displayDLL(DLL *items, FILE *fp) {
    NODE *item = items->head;
    if(item == NULL)
        return;
    fprintf(fp, "{");
    for(int i=0; i<items->size; i++) {
        items->display(item->value, fp);
        item = item->next;
        if(i == items->size-1)
            fprintf(fp, "}\n");
        else
            fprintf(fp, ",");
    }
}
void freeDLL(DLL *items) {
    NODE *item = items->head, *next = item->next;
    for(int i=0; i<items->size; i++) {
        items->free(item->value);
        free(item);
        item = next;
        if(next)
            next = next->next;
    }
}
