// created by Simone Schiavone at 20211007 09:01.
// @Università di Pisa
// Matricola 582418

#include "utils/serverutils.h"
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>


#define UNIX_PATH_MAX 108

#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}

void reset_socket(){
    unlink(socket_name);
}

//volatile sig_atomic_t term = 0; //FLAG SETTATO DAL GESTORE DEI SEGNALI DI TERMINAZIONE

int main(){     
    //-----Messaggio di benvenuto-----
    Welcome();

    //-----Configurazione del server-----
    DefaultConfiguration();
    PrintConfiguration();
    ScanConfiguration(config_file_path);
    PrintConfiguration();

    //Rimuoviamo socket relativi a precedenti computazioni del server
    reset_socket();
    atexit(reset_socket);

    if(LogFileAppend("Server acceso!\n")!=0){
        perror("Errore nella scrittura dell'evento nel file di log");
        return -1;
    }
    #if 0
    //-----Creazione del socket e setting dell'indirizzo-----
    int fd_s,fd_c; //file descriptor socket
    SYSCALL(fd_s,socket(AF_UNIX,SOCK_STREAM,0),"Errore nella 'socket'");
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
    strncpy(sa.sun_path,socket_name,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;

    int ret;
    SYSCALL(ret,bind(fd_s,(struct sockaddr*)&sa,sizeof(sa)),"Errore nella 'bind'");
    SYSCALL(ret,listen(fd_s,max_connections),"Errore nella 'listen'");

    int condition=1;

    while(1){
        printf("Server in attesa di una connessione...\n");
        if(LogFileAppend("Server in attesa di una connessione...\n")!=0){
            perror("Errore nella scrittura dell'evento nel file di log");
            return -1;
        }
        SYSCALL(fd_c,accept(fd_s,(struct sockaddr*)NULL,0),"Errore nella 'accept'");        
        if(LogFileAppend("Accettata una connessione sul fd %d\n",fd_c)!=0){
            perror("Errore nella scrittura dell'evento nel file di log");
            return -1;
        }
        condition=1;
        while(condition){
            printf("HEY\n");
            char* buff=(char*)calloc(128,sizeof(char));
            int n;
            SYSCALL(n,read(fd_c,buff,sizeof(buff)),"Errore nella read del msg <-");
            LogFileAppend("Ho ricevuto la seguente stringa: %s\n",buff);
            SYSCALL(n,write(fd_c,"OK",2),"Errore nella write della risposta ->");
            if(strncmp(buff,"stop",4)==0){
                close(fd_c);
                if(LogFileAppend("Chiusa la connessione sul fd %d\n",fd_c)!=0){
                    perror("Errore nella scrittura dell'evento nel file di log");
                    return -1;
                }
                condition=0;
            }
            free(buff);
        }

    }
    close(fd_s);
    pthread_t me=pthread_self();
    printf("T_ID:%ld\n",me);
    #endif

    if(InitializeStorage()==0)
        perror("Errore initialize storage!");

    stored_file* new=(stored_file*)malloc(sizeof(stored_file));
    stored_file* new1=(stored_file*)malloc(sizeof(stored_file));
    icl_hash_insert(storage,"pippo",new);
    icl_hash_insert(storage,"pluto",new1);
    icl_hash_dump(stdout,storage);
    if(DestroyStorage()==-1)
        perror("Errore destroy storage");
    
    //Chiudiamo il file di log
    if(fclose(logfile)!=0){
        perror("Errore in chiusura del file di log");
        return -1;
    }
    return 0;    
}
