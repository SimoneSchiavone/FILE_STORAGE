/*Realizzare un programma C che implementa un server che rimane sempre attivo 
in attesa di richieste da parte di uno o piu' processi client su una socket di 
tipo AF_UNIX. Ogni client richiede al server la trasformazione di tutti i 
caratteri di una stringa da minuscoli a maiuscoli (es. ciao –> CIAO). Per ogni 
nuova connessione il server lancia un thread POSIX che gestisce tutte le richieste
del client (modello “un thread per connessione” – i thread sono spawnati in modalità detached)
e quindi termina la sua esecuzione quando il client chiude la connessione. Per testare il 
programma, lanciare piu' processi client ognuno dei quali invia una o piu' richieste 
al server multithreaded.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include "client_utils/serverAPI.h"

#define SOCKNAME "./SocketFileStorage"

#define BUFFDIM 256
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}
#define CALLOC(r) r=(char*) calloc(BUFFDIM,sizeof(char)); if(r==NULL){ perror("calloc"); return EXIT_FAILURE;}

void Welcome();
void PrintAcceptedOptions();

int main(int argc,char** argv){
    char* socketname=NULL;
    int enable_printing=0;
    Welcome();

    //Parsing degli argomenti da linea di comando
    int opt;
    while((opt=getopt(argc,argv,"hf:w:W:D:r:R:d:t:l:u:c:p"))!=-1){
        switch (opt){
            case 'h':
                PrintAcceptedOptions();
                goto exit;
            case 'f':
                socketname=optarg;
                break;
            case 'w':
                break;
            case 'W':
                break;
            case 'D':
                break;
            case 'r':
                break;
            case 'R':
                break;
            case 'd':
                break;
            case 't':
                break;
            case 'l':
                break;
            case 'u':
                break;
            case 'c':
                break;
            case 'p':
                enable_printing=1;
                break;
            case '?':
            printf("Necessario argomento\n");
                break;
            default:
            printf("opzione sconosciuta\n");
                break;
        }
    }

    //DEBUGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
    printf("Enableprinting:%d\n",enable_printing);
    
    
    struct timespec a;
    a.tv_sec=15;

    if(socketname==NULL){
        printf("--->NULL socketname\n");
        if(openConnection(SOCKNAME,5000,a)==-1)
            return EXIT_FAILURE;
    }else{
        printf("--->NOT NULL socketname\n");
        if(openConnection(socketname,5000,a)==-1)
            return EXIT_FAILURE;
    }
    //char name[]="/Topolino\0";
    //printf("Dimensione: %ld Stringa: %s\n",strlen(name),name);
    openFile("topolino.txt\0",1,1);
    writeFile("topolino.txt\0",NULL);
    sleep(5);
    openFile("minnie.txt\0",1,1);
    writeFile("minnie.txt\0",NULL);
    /*
    char* buffer=(char*)calloc(BUFFDIM,sizeof(char));
    int n;
    SYSCALL(n,write(fd_connection,"Ciao",4),"Errore nella write");
    SYSCALL(n,read(fd_connection,buffer,BUFFDIM),"Errore nella read");
    printf("Ho ricevuto %s\n",buffer);
    memset(buffer,'\0',BUFFDIM);
    SYSCALL(n,write(fd_connection,"stop",4),"Errore nella write");
    SYSCALL(n,read(fd_connection,buffer,BUFFDIM),"Errore nella read");
    printf("Ho ricevuto %s\n",buffer);
    free(buffer);
    */
    closeConnection(socketname);
    printf("Sock: %s\n",socketname);
    exit:
        return EXIT_SUCCESS;
}

void Welcome(){
    system("clear");
    printf(" ______ _____ _      ______    _____ _______ ____  _____            _____ ______ \n");
    printf("|  ____|_   _| |    |  ____|  / ____|__   __/ __ \\|  __ \\     /\\   / ____|  ____|\n");
    printf("| |__    | | | |    | |__    | (___    | | | |  | | |__) |   /  \\ | |  __| |__ \n");
    printf("|  __|   | | | |    |  __|    \\___ \\   | | | |  | |  _  /   / /\\ \\| | |_ |  __|\n");
    printf("| |     _| |_| |____| |____   ____) |  | | | |__| | | \\ \\  / ____ \\ |__| | |____\n");
    printf("|_|    |_____|______|______| |_____/   |_|  \\____/|_|  \\_\\/_/    \\_\\_____|______|\n");
    printf("***Client File Storage ATTIVO***\n\n");
}

void PrintAcceptedOptions(){
    printf("*****Opzioni ACCETTATE*****\n");
    printf("\t '-f filename' \n\tspecifica il nome del socket AF_UNIX a cui connettersi\n\n");
    printf("\t '-w dirname[,n=0]'\n\t invia al server i file nella cartella ‘dirname’ Se la directory ‘dirname’ contiene\n\t altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ \n\t file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da \n\t inviare al server (tuttavia non è detto che il server possa scriverli tutti).\n\n");
    printf("\t '-W file1[,fil2]' \n\t lista di nomi di file da scrivere nel server separati da ','\n\n");
    printf("\t '-D dirname' \n\t specifica la cartella in memoria secondaria dove vengono memorizzati i file che il server\n\t rimuove a seguito di capacity misses ottenuti a causa di scritture richieste attraverso le \n\t opzioni -W e -w. L'opzione -D deve essere usata congiuntamente all'opzione -w o -W \n\t altrimenti è generato un errore. Se l'opzione -D non viene specificata tutti \n\t i file che il server invia verso il client in seguito a capacity misses vengono persi. \n\n");
    printf("\t '-r file1[,file2]' \n\t lista di nomi di file da leggere dal server separati da ','\n\n");
    printf("\t '-R [n=0]' \n\t tale opzione permette di leggere n file qualsiasi attualmente memorizzati nel server. se \n\t n=0 vengono letti tutti i file presenti nel server.\n\n");
    printf("\t '-d dirname' \n\t cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione -r o -R.\n\t L'opzione -d va usata congiuntamente a '-r' o '-R' altrimenti viene generato un errore; Se\n\t vengono utilizzate le opzioni '-r' o '-R' senza specificare l'opzione '-d' i file \n\t letti non vengono memorizzati sul disco\n\n");
    printf("\t '-t time' \n\t tempo in millisecondi che intercorre tra l'invio di due richieste successive al server. \n\t Se time=0 non vi è alcun ritardo tra l'invio di due richieste consecutive.\n\n");
    printf("\t '-l file1[,file2]' \n\t lista di file su cui acquisire la mutua esclusione\n\n");
    printf("\t '-u file1[,file2]' \n\t lista di fule su cui rilasciare la mutua esclusione\n\n");
    printf("\t '-c file1[,file2]' \n\t lista di file da rimuovere nel server se presenti\n\n");
    printf("\t '-p' \n\t abilita le stampe sullo standard output per ogni operazione\n");
    printf("*****************\n");
}