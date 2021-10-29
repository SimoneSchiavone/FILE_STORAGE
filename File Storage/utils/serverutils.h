// created by Simone Schiavone at 20211007 21:52.
// @Università di Pisa
// Matricola 582418
//#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "data_structures.h"

#define LOGFILEAPPEND(...){ \
            if(LogFileAppend(__VA_ARGS__)!=0){ \
                perror("Errore nella scrittura dell'evento nel file di log"); \
                return -1; \
            } \
}
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}

/*-----Parametri di configurazione del server-----*/
char  config_file_path[128]; //path del file testuale di configurazione
int n_workers; // numero thread workers del modello Manager-Worker
int max_files_num;  //numero massimo di files
int max_files_dim;  //dimensione massima dei files in MBytes
char socket_name[128]; //nome del socket AF_UNIX
char logfilename[128]; //nome del file di log
int max_connections; //numero massimo di connessioni contemporanee supportate

/*-----File-----*/
typedef struct stored_file{
    //TODO: Il path potrebbe essere la chiave mentre i restanti i dati satellite
    char* content; //contenuto
    size_t size;
    int fd_owner; //fd della connessione con il proprietario del file
    //Lock? Read/Write? Locked? Size? Lista di chi ha aperto il file?
}stored_file;
void print_stored_file_info(FILE* stream,void* file);

typedef struct response{
    int code;
    char message[100];
}response;

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

/*-----Hash Table Storage-------*/
hash_table_t* storage;
int InitializeStorage();
int DestroyStorage();
response OpenFile(char* pathname,int o_create,int o_lock,int fd_owner);

/*-----Thread Pool-----*/
pthread_t* threadpool;
IntLinkedList queue; //FORSE VA INIZIALIZZATA A NULL