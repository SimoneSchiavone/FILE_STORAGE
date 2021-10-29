#ifndef data_structures_h
#define data_structures_h

#include <stdio.h>

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

typedef struct entry_s {
    void* key;
    void* data;
    struct entry_s* next;
} entry_t;

typedef struct hash_s {
    int nbuckets;
    int nentries;
    entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} hash_t;

typedef hash_t hash_table_t;
typedef entry_t hash_table_entry;
pthread_mutex_t mutex_storage;

hash_t* hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );
void* hash_find(hash_t *, void* );
int hash_insert(hash_t *, void*, void *);
entry_t* hash_update_insert(hash_t *, void*, void *, void **);
int hash_destroy(hash_t *, void (*)(void*), void (*)(void*));
int hash_dump(FILE* stream, hash_t* ht,void (print_data_info)(FILE*, void*));
int hash_delete( hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );
/* simple hash function */
unsigned int hash_pjw(void* key);
/* compare function */
int string_compare(void* a, void* b);


#define hash_foreach(ht, tmpint, tmpent, kp, dp)    \
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
int list_push_terminators(Node** head_ref,int nr_workers);
int list_pop(Node** head_ref);
void list_destroy(struct Node* head_ref);
void list_print(struct Node*);
#endif 
