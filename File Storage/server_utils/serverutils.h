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

#define LOGFILEAPPEND(...){ \
            if(LogFileAppend(__VA_ARGS__)!=0){ \
                fprintf(stderr,"Errore nella scrittura dell'evento nel file di log"); \
            } \
}
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}
/*-----Parametri di configurazione del server-----*/
char config_file_path[128]; //path del file testuale di configurazione
int n_workers; // numero thread workers del modello Manager-Worker
int files_bound; //numero massimo di files
int data_bound; //dimensione massima dei files in MBytes
int replacement_policy; //politica di rimpiazzamento dei file nello storage
char socket_name[128]; //nome del socket AF_UNIX
char logfilename[128]; //nome del file di log
int max_connections; //numero massimo di connessioni contemporanee supportate
int active_connections; //numero di connessioni attive

/*-----File-----*/
typedef struct stored_file{
    char* content; //contenuto
    size_t size; //dimensione
    struct timeval creation_time; //tempo di creazione, necessario per la politica FIFO
    struct timeval last_operation; //tempo dell'ultima modifica

    int fd_holder; //fd della connessione del proprietario del file; Se -1 il file e' sbloccato
    pthread_mutex_t mutex_file; //lock del file
    pthread_cond_t is_unlocked; //variabile di condizione per l'attesa dello sblocco del file

    int clients_waiting;
    int to_delete;
    pthread_cond_t is_deletable;
}stored_file;
void print_stored_file_info(FILE* stream,void* file);

/*-----File di log per la registrazione degli eventi-----*/
FILE* logfile;
pthread_mutex_t mutex_logfile; //mutex per l'acquisizione in mutua esclusione del file di log
int LogFileAppend(char* event, ...);

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
    char message[100];
}response;


/*-----Hash Table Storage-------*/
hash_table_t* storage;
pthread_mutex_t mutex_storage;
int not_empty_files_num;
int data_size;
int max_data_size;
int max_data_num;
int nr_of_replacements;
int InitializeStorage();
int DestroyStorage();

/*-----Thread Pool-----*/
pthread_t* threadpool;
int terminated_workers;
pthread_mutex_t term_var;
IntLinkedList queue;





