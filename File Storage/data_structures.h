#ifndef data_structures_h
#define data_structures_h

#include <stdio.h>
#include <pthread.h>

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

//Entry dell'hash table
typedef struct entry_s {
    void* key;
    void* data;
    struct entry_s* next;
} entry_t;

//Tipo di una hash table
typedef struct hash_s {
    int nbuckets;
    int nentries;
    entry_t **buckets;
    unsigned int (*hash_function)(void*);
    int (*hash_key_compare)(void*, void*);
} hash_t;

typedef hash_t hash_table_t;
typedef entry_t hash_table_entry;

//Operazioni sulla tabella hash
hash_t* hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) );
void* hash_find(hash_t *, void* );
int hash_insert(hash_t *, void*, void *);
entry_t* hash_update_insert(hash_t *, void*, void *, void **);
int hash_destroy(hash_t *, void (*)(void*), void (*)(void*));
int hash_dump(FILE* stream, hash_t* ht,void (print_data_info)(FILE*, void*));
int hash_delete( hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*) );

#define hash_foreach(ht, tmpint, tmpent, kp, dp)    \
    for (tmpint=0;tmpint<ht->nbuckets; tmpint++)        \
        for (tmpent=ht->buckets[tmpint];                                \
             tmpent!=NULL&&((kp=tmpent->key)!=NULL)&&((dp=tmpent->data)!=NULL); \
             tmpent=tmpent->next)

/* simple hash function */
unsigned int hash_pjw(void* key);
/* compare function */
int string_compare(void* a, void* b);


#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

// created by Simone Schiavone at 20211025 15:29.
// @Università di Pisa
// Matricola 582418

/* Nodo di una lista di interi*/
typedef struct Node{ 
    int data; 
    struct Node *next; 
}Node; 

typedef Node* IntLinkedList;

//Lista sincronizzata
pthread_mutex_t mutex_list;
pthread_cond_t list_not_empty;
int concurrent_list_push(Node** head_ref, int new_data);
int concurrent_list_push_terminators(Node** head_ref,int nr_workers);
int concurrent_list_pop(Node** head_ref);
void concurrent_list_destroy(struct Node* head_ref);

//Lista non sincronizzata
void list_print(struct Node*);
void list_destroy(Node* head_ref);
int is_in_list(Node* head_ref, int to_search);
int list_remove(Node** head_ref, int to_remove);
int list_push(Node** head_ref, int new_data);
#endif 
