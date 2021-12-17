/**
 * @file hash.c
 *
 * Dependency free hash table implementation.
 *
 * This simple hash table implementation should be easy to drop into
 * any other peice of code, it does not depend on anything else :-)
 * 
 * @author Jakub Kurzak
 */
/* $Id: hash.c 2838 2011-11-22 04:25:02Z mfaverge $ */
/* $UTK_Copyright: $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "data_structures.h"
#include <limits.h>


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
unsigned int hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;
    if(!datum) return 0;
    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

int string_compare(void* a, void* b){
    return (strcmp( (char*)a, (char*)b ) == 0);
}

/**
 * Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

hash_t * hash_create( int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*) ){
    hash_t *ht;
    int i;
    ht = (hash_t*) malloc(sizeof(hash_t));
    if(!ht) return NULL;
    ht->nentries = 0;
    ht->buckets = (entry_t**)malloc(nbuckets * sizeof(entry_t*));
    if(!ht->buckets) return NULL;
    ht->nbuckets = nbuckets;
    for(i=0;i<ht->nbuckets;i++)
        ht->buckets[i] = NULL;
    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;
    return ht;
}

/**
 * Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

void * hash_find(hash_t *ht, void* key){
    entry_t* curr;
    unsigned int hash_val;
    if(!ht || !key) return NULL;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next)
        if ( ht->hash_key_compare(curr->key, key)){
            return(curr->data);
        }
    return NULL;
}

/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns 1 if parameters are null, 2 if the key already exists, 3 in case of
 * malloc problems, 0 if OK
 */

int hash_insert(hash_t *ht, void* key, void *data){
    entry_t *curr;
    unsigned int hash_val;
    if(!ht || !key){
        perror("Parametri NULL");
        return 1;
    } 
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    for (curr=ht->buckets[hash_val]; curr != NULL; curr=curr->next){
        if ( ht->hash_key_compare(curr->key, key)){
            printf("Il file è già presente nello storage\n");
            return 2; /* key already exists */
        }
    }
    /* if key was not found */
    curr = (entry_t*)malloc(sizeof(entry_t));
    if(!curr){
        perror("Malloc nuova entry hash table");
        return 3;
    }
    curr->key = key;
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */
    ht->buckets[hash_val] = curr;
    ht->nentries++;
    return 0;
}

/**
 * Replace entry in hash table with the given entry.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 * @param olddata -- pointer to the old item's data (set upon return)
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

entry_t * hash_update_insert(hash_t *ht, void* key, void *data, void **olddata){
    entry_t *curr, *prev;
    unsigned int hash_val;
    if(!ht || !key) return NULL;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    /* Scan bucket[hash_val] for key */
    for (prev=NULL,curr=ht->buckets[hash_val]; curr != NULL; prev=curr, curr=curr->next)
        /* If key found, remove node from list, free old key, and setup olddata for the return */
        if ( ht->hash_key_compare(curr->key, key)) {
            if (olddata != NULL) {
                *olddata = curr->data;
                free(curr->key);
            }

            if (prev == NULL)
                ht->buckets[hash_val] = curr->next;
            else
                prev->next = curr->next;
        }

    /* Since key was either not found, or found-and-removed, create and prepend new node */
    curr = (entry_t*)malloc(sizeof(entry_t));
    if(curr == NULL){
        perror("Malloc new node");
        return NULL; /* out of memory */
    }

    curr->key = key;
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;

    if(olddata!=NULL && *olddata!=NULL)
        *olddata = NULL;
    return curr;
}

/**
 * Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int hash_delete(hash_t *ht, void* key, void (*free_key)(void*), void (*free_data)(void*)){
    entry_t *curr, *prev;
    unsigned int hash_val;
    if(!ht || !key) return -1;
    hash_val = (* ht->hash_function)(key) % ht->nbuckets;
    prev = NULL;
    for (curr=ht->buckets[hash_val]; curr != NULL; )  {
        if ( ht->hash_key_compare(curr->key, key)) {
            if (prev == NULL) {
                ht->buckets[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            ht->nentries--;
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/**
 * Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int hash_destroy(hash_t *ht, void (*free_key)(void*), void (*free_data)(void*)){
    entry_t *bucket, *curr, *next;
    int i;
    if(!ht) return -1;
    for (i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr=bucket; curr!=NULL; ) {
            next=curr->next;
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            free(curr);
            curr=next;
        }
    }
    if(ht->buckets) free(ht->buckets);
    if(ht) free(ht);
    return 0;
}

/**
 * Dump the hash table's contents to the given file pointer.
 *
 * @param stream -- the file to which the hash table should be dumped
 * @param ht -- the hash table to be dumped
 * @param print_data_info -- void function that 
 * @returns 0 on success, -1 on failure.
 */
 
int hash_dump(FILE* stream, hash_t* ht,void (print_data_info)(FILE*, void*)){
    entry_t *bucket, *curr;
    int i;
    if(!ht) return -1;
    //fprintf(stream,"-----HASH DUMP-----\n");
    for(i=0; i<ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key){
                fprintf(stream, "Key: %s\n\tLocation: %p\n", (char *)curr->key, curr->data);
                print_data_info(stream,curr->data);
            }
            curr=curr->next;
        }
    }
    return 0;
}

// created by Simone Schiavone at 20211025 15:28.
// @Università di Pisa
// Matricola 582418

/* Function to add a node at the beginning of Linked List. 
   This function expects a pointer to the data to be added 
   and size of the data type */
int list_push(Node** head_ref, int new_data){ 
    // Allocate memory for node 
    Node* new_node = (Node*)malloc(sizeof(Node)); 
    if(new_node==NULL){
        perror("Errore nella 'malloc' del nuovo nodo della lista");
        return -1;
    }
    //Preparation of the new node
    new_node->data  = new_data; 
    new_node->next = (*head_ref); 
    
    // Change head pointer as new node is added at the beginning 
    pthread_mutex_lock(&mutex_list);
    (*head_ref)    = new_node; 
    pthread_cond_signal(&list_not_empty);
    pthread_mutex_unlock(&mutex_list);
    return 0;
} 

void list_destroy(Node* head_ref){
    pthread_mutex_lock(&mutex_list);
    Node* curr=head_ref;
    Node* tmp;
    while(curr != NULL){
        tmp=curr->next;
        free(curr);
        curr=tmp;
    }
    pthread_mutex_unlock(&mutex_list);
}

int list_pop(Node** head_ref){
    //printf("[WORKER %ld-ListPop] Inizio procedura\n",pthread_self());
    pthread_mutex_lock(&mutex_list);
    //printf("[WORKER %ld-ListPop] lock preso\n",pthread_self());
    //While the list doesn't containt an element wait for someone push a new one
    while(*head_ref==NULL){
        //printf("[WORKER %ld-ListPop] aspetto la condizione\n",pthread_self());
        pthread_cond_wait(&list_not_empty,&mutex_list);
        //printf("[WORKER %ld-ListPop] mi sono svegliato\n",pthread_self());
        fflush(stdout);
    }
    //Extraction of the element at the top of the list
    Node* extracted= *head_ref;
    (*head_ref)=(*head_ref)->next;
    pthread_mutex_unlock(&mutex_list);
    //printf("[WORKER %ld-ListPop] lock rilasciato\n",pthread_self());
    
    //Free the node and return the int value
    int data=extracted->data;
    free(extracted);
    return data;
}

void list_print(Node* head_ref){
    Node* curr=head_ref;
    printf("List:\t");
    while(curr!=NULL){
        printf("%d --> ",curr->data);
        curr=curr->next;
    }
    printf("NULL\n");
}

int list_push_terminators(Node** head_ref,int n_workers){
    printf("[WORKER %ld-ListPushTerminators] Inizio procedura\n",pthread_self());
    list_print(*head_ref);
    //Lock acquire
    pthread_mutex_lock(&mutex_list);
    printf("[WORKER %ld-ListPushTerminator] lock preso\n",pthread_self());
    for(int i=0;i<n_workers;i++){
        Node* terminator=(Node*)malloc(sizeof(Node));
        if(terminator==NULL)
            return -1;
        terminator->data=-1;
        terminator->next=(*head_ref); 
        (*head_ref)=terminator; 
    }
    printf("[WORKER %ld-ListPushTerminator] inseriti %d terminatori\n",pthread_self(),n_workers);
    list_print(*head_ref);
    pthread_cond_signal(&list_not_empty);
    pthread_mutex_unlock(&mutex_list);
    printf("[WORKER %ld-ListPushTerminator] signal & unlock fatte, esco\n",pthread_self());
    return 0;
}

int list_push_terminators_end(Node** head_ref,int n_workers){
    printf("[WORKER %ld-ListPushTerminatorsEnd] Inizio procedura\n",pthread_self());
    list_print(*head_ref);
    //Lock acquire
    pthread_mutex_lock(&mutex_list);
    printf("[WORKER %ld-ListPushTerminator] lock preso\n",pthread_self());
    //Scorro la lista per aggiungere alla fine
    Node* corr=(*head_ref);
    if((*head_ref)!=NULL){
        while (corr->next){
            corr=corr->next;
        }
        for(int i=0;i<n_workers;i++){
            Node* terminator=(Node*)malloc(sizeof(Node));
            if(terminator==NULL)
                return -1;
            corr->next=terminator;
            terminator->data=-1;
            terminator->next=NULL; 
            corr=corr->next; 
        }
        printf("[WORKER %ld-ListPushTerminator] inseriti %d terminatori in fondo\n",pthread_self(),n_workers);
    }else{
        for(int i=0;i<n_workers;i++){
            Node* terminator=(Node*)malloc(sizeof(Node));
            if(terminator==NULL)
                return -1;
            terminator->data=-1;
            terminator->next=(*head_ref); 
            (*head_ref)=terminator; 
        }
        printf("[WORKER %ld-ListPushTerminator] inseriti %d terminatori in testa\n",pthread_self(),n_workers);
    }
    list_print(*head_ref);
    pthread_cond_signal(&list_not_empty);
    pthread_mutex_unlock(&mutex_list);
    printf("[WORKER %ld-ListPushTerminator] signal & unlock fatte, esco\n",pthread_self());
    return 0;
}

