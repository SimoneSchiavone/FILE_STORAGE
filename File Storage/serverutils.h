// created by Simone Schiavone at 20211007 21:52.
// @Università di Pisa
// Matricola 582418

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include "data_structures.h"


#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); return -1;}
#define SYSCALL_VOID(r,c,e) if((r=c)==-1) {perror(e); return;}
#define SYSCALL_RESPONSE(w,c,e) if((w=c)==-1) {perror(e); r.code=-1; sprintf(r.message,"Errore!");}

/*-----Parametri di configurazione del server-----*/
char config_file_path[128]; //path del file testuale di configurazione
int n_workers; // numero thread workers del modello Manager-Worker
int files_bound; //numero massimo di files
int data_bound; //dimensione massima dei files in MBytes
int replacement_policy; //politica di rimpiazzamento dei file nello storage
char socket_name[128]; //nome del socket AF_UNIX
char logfilename[128]; //nome del file di log
int max_connections_bound; //numero massimo di connessioni contemporanee supportate

/*-----File-----*/
typedef struct stored_file{
    char* content; //contenuto
    size_t size; //dimensione

    Node* opened_by; //lista di client che hanno aperto il file
    
    struct timeval creation_time; //tempo di creazione del file
    struct timeval last_operation; //tempo dell'ultima operazione sul file

    int fd_holder; //fd della connessione del proprietario del file; Se -1 il file e' sbloccato
    pthread_mutex_t mutex_file; //lock del file
    pthread_cond_t is_unlocked; //variabile di condizione per l'attesa dello sblocco del file

    int clients_waiting; //numero di client in attesa di acquisire la lock sul client
    int to_delete; //flag che indica che il file deve essere eliminato
    pthread_cond_t is_deletable; //variabile di condizione per attesa delle condizioni necessarie all'eliminazione
}stored_file;
void print_stored_file_info(FILE* stream,void* file);

/*-----File di log per la registrazione degli eventi-----*/
FILE* logfile;
pthread_mutex_t mutex_logfile; //mutex per l'acquisizione in mutua esclusione del file di log
int LogFileAppend(char* event, ...);
#define LOGFILEAPPEND(...){ \
            if(LogFileAppend(__VA_ARGS__)!=0){ \
                fprintf(stderr,"Errore nella scrittura dell'evento nel file di log"); \
            } \
}

/*-----Messaggio di benvenuto-----*/
void Welcome();

/*-----Gestione del file di configurazione-----*/
int ScanConfiguration();
void DefaultConfiguration();
void PrintConfiguration();

/*-----Esecuzione dei comandi-----*/
int ExecuteRequest(int fun,int fd);
typedef struct response{
    int code;
    char message[256];
}response;


/*-----Hash Table Storage-------*/
hash_table_t* storage;
pthread_mutex_t mutex_storage; //mutex sullo storage
int not_empty_files_num; //numero di file non vuoti
int data_size; //dimensione complessiva dei dati memorizzati
int max_data_size; //dimensione complessiva massima dei dati memorizzati
int max_data_num; //numero massimo di file memorizzati
int nr_of_replacements; //numero di attivazioni dell'algoritmo di rimpiazzamento
int InitializeStorage();
int DestroyStorage();

/*-----Thread Pool-----*/
pthread_t* threadpool; //array di thread worker
IntLinkedList queue; //lista di lavoro dei worker

/*Funzioni per evitare scritture/letture parziali*/
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);


