/**
 * @file
 *
 * Header file for icl_hash routines.
 *
 */
/* $Id$ */
/* $UTK_Copyright: $ */

#ifndef data_structures_h
#define icl_hash_h

#include <stdio.h>

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

typedef struct icl_entry_s {
    void* key;
    void *data;
    struct icl_entry_s* next;
} icl_entry_t;

typedef struct icl_hash_s {
    int nbuckets;
    int nentries;
    icl_entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} icl_hash_t;

typedef icl_hash_t hash_table_t;
typedef icl_entry_t hash_table_entry;

icl_hash_t *
icl_hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );

void
* icl_hash_find(icl_hash_t *, void* );

icl_entry_t
* icl_hash_insert(icl_hash_t *, void*, void *),
    * icl_hash_update_insert(icl_hash_t *, void*, void *, void **);

int
icl_hash_destroy(icl_hash_t *, void (*)(void*), void (*)(void*)),
    icl_hash_dump(FILE *, icl_hash_t *);

int icl_hash_delete( icl_hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );

/* simple hash function */
unsigned int
hash_pjw(void* key);

/* compare function */
int 
string_compare(void* a, void* b);


#define icl_hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

// created by Simone Schiavone at 20211025 15:29.
// @Università di Pisa
// Matricola 582418

/* A linked list of integer */
typedef struct Node{ 
    // Any data type can be stored in this node 
    int data; 
    struct Node *next; 
}Node; 

typedef Node* IntLinkedList;
pthread_mutex_t mutex_list;
pthread_cond_t list_not_empty;
int list_push(Node** head_ref, int new_data);
int list_pop(Node** head_ref);
void list_destroy(struct Node* head_ref);

#endif /* icl_hash_h */
