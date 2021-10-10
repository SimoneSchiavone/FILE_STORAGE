// created by Simone Schiavone at 20211007 21:52.
// @Università di Pisa
// Matricola 582418
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/*-----Parametri di configurazione del server-----*/

char  config_file_path[128]; //path del file testuale di configurazione
int n_workers; // numero thread workers del modello Manager-Worker
int max_files_num;  //numero massimo di files
int max_files_dim;  //dimensione massima dei files in MBytes
char socket_name[128]; //nome del socket AF_UNIX
char logfilename[128]; //nome del file di log
int max_connections; //numero massimo di connessioni contemporanee supportate

/*-----File di log per la registrazione degli eventi-----*/
FILE* logfile;
pthread_mutex_t mutex_logfile; //mutex per l'acquisizione in mutua esclusione del file di log


/*------------------------------------------------*/
void Welcome();
int ScanConfiguration();
void DefaultConfiguration();
void PrintConfiguration();
int LogFileAppend(char* event, ...);
