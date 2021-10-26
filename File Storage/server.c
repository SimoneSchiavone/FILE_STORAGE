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
#define MAX_FD(a) if(a>fd_max) fd_max=a;

int Search_New_Max_FD(fd_set set,int maxfd){
    for(int i=maxfd-1;i>=0;i--){
        if(FD_ISSET(i,&set))
            return i;
    }
    return -1;
}

void reset_socket(){
    unlink(socket_name);
}

void* WorkerFun(void* p){
    printf("[WORKER %ld] Ho iniziato la mia esecuzione!\n",pthread_self());
    int pipe_fd=*((int*)p);
    int condition=1;
    while(condition){
        //Estraiamo un fd dalla coda
        printf("[WORKER %ld] aspetto di estrarre qualcuno dalla coda\n",pthread_self());
        int current_fd=list_pop(&queue);
        printf("[WORKER %ld] Ho estratto dalla coda il fd %d!\n",pthread_self(),current_fd);
        //Allochiamo il buffer per accogliere il messaggio
        char* buff=(char*)calloc(128,sizeof(char));
        int n;
        SYSCALL(n,read(current_fd,buff,sizeof(buff)),"Errore nella 'read' del msg <-");
        if(LogFileAppend("[WORKER %ld] Ho ricevuto la seguente stringa: %s\n",pthread_self(),buff)==-1){
            perror("Errore nella scrittura del logfile");
            break;
        }
        printf("[WORKER %ld] Ho ricevuto la seguente stringa: %s\n",pthread_self(),buff);
        SYSCALL(n,write(current_fd,"OK",2),"Errore nella 'write' del codice OK al client");
        printf("[WORKER %ld] Ho risposto OK al client connesso al fd %d\n",pthread_self(),current_fd);

        SYSCALL(n,write(pipe_fd,&current_fd,4),"Errore nella 'write' del fd sulla pipe");
        if(strncmp(buff,"stop",4)==0){
            int w=1;
            SYSCALL(n,write(pipe_fd,&w,4),"Errore nella 'write' del flag sulla pipe");
            //DEBUGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
            condition=0;
        }else{
            int w=0;
            SYSCALL(n,write(pipe_fd,&w,4),"Errore nella 'write' del flag sulla pipe");
        }
        free(buff);
    }
    fflush(stdout);
    return NULL;
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
    atexit(reset_socket); //Questa procedura atexit funziona???????????????????????????

    LOGFILEAPPEND("Server acceso!\n");

    //-----Creazione del socket e setting dell'indirizzo-----
    int listen_fd; //file descriptor socket
    int connection_fd;
    SYSCALL(listen_fd,socket(AF_UNIX,SOCK_STREAM,0),"Errore nella 'socket'");
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
    strncpy(sa.sun_path,socket_name,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;

    int ret;
    SYSCALL(ret,bind(listen_fd,(struct sockaddr*)&sa,sizeof(sa)),"Errore nella 'bind'");
    SYSCALL(ret,listen(listen_fd,max_connections),"Errore nella 'listen'");

    //-----Creazione threadpool-----
    threadpool=malloc(n_workers*sizeof(pthread_t));
    queue=NULL;
    if(threadpool==NULL){
        perror("Errore nella 'malloc' del threadpool");
        return EXIT_FAILURE;
    }

    //-----Creazione Pipe-----
    int wtm_pipe[2]; //WorkerToMaster_Pipe utilizzata per la comunicazione dal thread worker verso il server/manager
    int tmp;
    SYSCALL(tmp,pipe(wtm_pipe),"Errore nella creazione della pipe");

    //-----Creazione Thread Workers-----
    for(int i=0;i<n_workers;i++){
            SYSCALL(threadpool[i],pthread_create(&threadpool[i],NULL,WorkerFun,(void*)&wtm_pipe[1]),"Errore nella creazione del thread");
            //TODO: implementare la funzione del workerthread valutando opportuni argomenti
    }

    int fd_max=0;
    MAX_FD(listen_fd); // tengo traccia del file descriptor con id piu' grande
    
    //-----Registrazione del welcome socket e della pipe
    fd_set set,rdset;
    FD_ZERO(&set);
    FD_SET(listen_fd,&set);
    MAX_FD(wtm_pipe[0]);
    FD_SET(wtm_pipe[0],&set);

    while(1){
        printf("[MAIN] Server in attesa di una connessione...\n");
        LOGFILEAPPEND("[MAIN] Server in attesa di una connessione...\n");
        //Copio il set nella variabile per la select
        rdset=set;
        if(select(fd_max+1,&rdset,NULL,NULL,NULL)==-1){
            perror("Errore nella 'select'");
            goto exit;
        }

        //Cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
        for(int i=0;i<=fd_max;i++){
            if(FD_ISSET(i,&rdset)){ 
                if(i==listen_fd){ //è una nuova richiesta di connessione! 
                    SYSCALL(connection_fd,accept(listen_fd,(struct sockaddr*)NULL,0),"Errore nella 'accept'");        
                    printf("[MAIN] Accettata una connessione sul fd %d\n",connection_fd);
                    LOGFILEAPPEND("[MAIN] Accettata una connessione sul fd %d\n",connection_fd);
                    FD_SET(connection_fd,&set);
                    MAX_FD(connection_fd);
                }else if (i==wtm_pipe[0]){ //è la pipe di comunicazione Worker to Manager che passa l'id del fd da reinserire in lista
                    int received_fd;
                    int byte_read;
                    int finished;
                    SYSCALL(byte_read,read(wtm_pipe[0],&received_fd,4),"Errore nella 'read' del fd restituito dal worker");
                    SYSCALL(byte_read,read(wtm_pipe[0],&finished,4),"Errore nella 'read' del flag di terminazione client proveniente dal worker");
                    if(finished){
                        printf("[MAIN] Il client sul fd %d ha eseguito una operazione ed è TERMINATO\n",received_fd);
                        printf("[MAIN] Chiudiamo la connessione sul fd %d\n",received_fd);
                        FD_CLR(received_fd,&set);
                        Search_New_Max_FD(set,fd_max);
                        int e;
                        SYSCALL(e,close(received_fd),"Errore nella chiusura del fd restituito dal worker");
                    }else{
                        printf("[MAIN] Il client sul fd %d ha eseguito una operazione e non è terminato\n",received_fd);
                        FD_SET(received_fd,&set);
                        MAX_FD(received_fd);
                    }
                }else{ //è un client pronto in lettura
                    int e;
                    printf("[MAIN] Il client sul fd %d è pronto in lettura\n",i);
                    SYSCALL(e,list_push(&queue,i),"Errore nella 'list_push' di un fd");
                    printf("[MAIN] Il client sul fd %d è stato inserito in coda\n",i);
                    FD_CLR(i,&set);
                }
            }
        }
     
    }
    exit:
    close(listen_fd);

    LOGFILEAPPEND("Server spento!\n");

    //Chiudiamo il file di log
    if(fclose(logfile)!=0){
        perror("Errore in chiusura del file di log");
        return -1;
    }

    return 0;    
}
