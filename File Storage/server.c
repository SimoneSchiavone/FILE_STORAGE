// created by Simone Schiavone at 20211007 09:01.
// @Università di Pisa
// Matricola 582418

#include "server_utils/serverutils.h"
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>

#define UNIX_PATH_MAX 108
#define CHECKRETURNVALUE(r,command,e,t) if((r=command)!=0) {perror(e); t;}
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

void* WorkerFun(void* p);
void* SignalHandlerFun(void* arg);
void Welcome();

volatile sig_atomic_t graceful_term=0; //FLAG SETTATO DAL GESTORE DEI SEGNALI DI TERMINAZIONE
volatile sig_atomic_t forced_term=0;

int main(){     
    int ctrl;

    //-----Messaggio di benvenuto-----
    Welcome();

    //-----Configurazione del server-----
    DefaultConfiguration();
    PrintConfiguration();
    ScanConfiguration(config_file_path);
    PrintConfiguration();

    //-----Inizializzazione dello Storage
    CHECKRETURNVALUE(ctrl,InitializeStorage(),"Errore inizializzando lo storage",goto exit);

    //Rimuoviamo socket relativi a precedenti computazioni del server
    reset_socket();
    atexit(reset_socket);

    LOGFILEAPPEND("Server acceso!\n");

    //-----Gestione dei segnali-----
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); 
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGHUP);
    CHECKRETURNVALUE(ctrl,pthread_sigmask(SIG_BLOCK,&mask,NULL),"Errore in 'pthread_sigmask",goto exit);
    
    struct sigaction s;
    memset(&s,0,sizeof(s));
    s.sa_handler=SIG_IGN;
    CHECKRETURNVALUE(ctrl,sigaction(SIGPIPE,&s,NULL),"Errore in 'sigaction",goto exit);
    pthread_t signal_handler_thread;
    CHECKRETURNVALUE(ctrl,pthread_create(&signal_handler_thread,NULL,SignalHandlerFun,&mask),"Errore nella creazione del thread signal handler",goto exit);



    //-----Creazione del socket e setting dell'indirizzo-----
    int listen_fd; //file descriptor socket
    int connection_fd;
    SYSCALL(listen_fd,socket(AF_UNIX,SOCK_STREAM,0),"Errore nella 'socket'");
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
    strncpy(sa.sun_path,socket_name,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;

    SYSCALL(ctrl,bind(listen_fd,(struct sockaddr*)&sa,sizeof(sa)),"Errore nella 'bind'");
    SYSCALL(ctrl,listen(listen_fd,max_connections),"Errore nella 'listen'");

    //-----Creazione threadpool-----
    threadpool=(pthread_t*)malloc(n_workers*sizeof(pthread_t));
    queue=NULL;
    if(threadpool==NULL){
        perror("Errore nella 'malloc' del threadpool");
        goto exit;
    }

    //-----Creazione Pipe-----
    int wtm_pipe[2]; //WorkerToMaster_Pipe utilizzata per la comunicazione dal thread worker verso il server/manager
    int tmp;
    SYSCALL(tmp,pipe(wtm_pipe),"Errore nella creazione della pipe");

    //-----Creazione Thread Workers-----
    pthread_t t;
    for(int i=0;i<n_workers;i++){
            //CHECKRETURNVALUE(threadpool[i],pthread_create(&threadpool[i],NULL,WorkerFun,(void*)&wtm_pipe[1]),"Errore nella creazione del thread",goto exit);
            CHECKRETURNVALUE(ctrl,pthread_create(&t,NULL,WorkerFun,(void*)&wtm_pipe[1]),"Errore nella creazione del thread",goto exit);
            threadpool[i]=t;
    }

    int fd_max=0;
    MAX_FD(listen_fd); // tengo traccia del file descriptor con id piu' grande
    
    //-----Registrazione del welcome socket e della pipe
    fd_set set,rdset;
    FD_ZERO(&set);
    FD_SET(listen_fd,&set);
    MAX_FD(wtm_pipe[0]);
    FD_SET(wtm_pipe[0],&set);

    terminated_workers=0;
    active_connections=0;
    pthread_mutex_init(&term_var,NULL);

    while(1){
        printf("[MAIN] Server in attesa di una connessione...\n");
        //Copio il set nella variabile per la select
        rdset=set;

        pthread_mutex_lock(&term_var);
        //printf("Dopo select -> Terminated = %d\n",terminated);
        if(terminated_workers==n_workers){
            pthread_mutex_unlock(&term_var);
            printf("SONO USCITO PRIMA LA SELECT\n");
            break;
        }
        pthread_mutex_unlock(&term_var);

        if(select(fd_max+1,&rdset,NULL,NULL,NULL)==-1){
            perror("Errore nella 'select'");
            goto exit;
        }

        pthread_mutex_lock(&term_var);
        //printf("Dopo select -> Terminated = %d\n",terminated);
        if(terminated_workers==n_workers){
            pthread_mutex_unlock(&term_var);
            printf("SONO USCITO DOPO LA SELECT\n");
            break;
        }
        pthread_mutex_unlock(&term_var);

        /*
        pthread_mutex_lock(&term_var);
        if(graceful_term){
            printf("[MAIN] Il server non puo' accettare ulteriori connessioni!\n");
            pthread_mutex_unlock(&term_var);
            FD_CLR(listen_fd,&set);
            Search_New_Max_FD(set,fd_max);
            close(listen_fd);
            rdset=set;
            select(fd_max+1,&rdset,NULL,NULL,NULL);
        }
        pthread_mutex_unlock(&term_var);*/

        //Cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
        for(int i=0;i<=fd_max;i++){
            if(FD_ISSET(i,&rdset)){ 
                if(i==listen_fd){ //è una nuova richiesta di connessione! 
                    SYSCALL(connection_fd,accept(listen_fd,(struct sockaddr*)NULL,0),"Errore nella 'accept'");
                    active_connections++;
                    printf("[MAIN] Accettata una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
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
                        if(received_fd!=-1){
                            printf("[MAIN] Il client sul fd %d ha eseguito una operazione ed è TERMINATO\n",received_fd);
                            FD_CLR(received_fd,&set);
                            Search_New_Max_FD(set,fd_max);
                            int e;
                            SYSCALL(e,close(received_fd),"Errore nella chiusura del fd restituito dal worker");
                            active_connections--;
                            printf("[MAIN] Chiusa la connessione sul fd %d - Connessioni attive %d\n",received_fd,active_connections);
                        }else{
                            //pthread_cond_signal(&list_not_empty);
                        }
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
        printf("\n##### Procedura di uscita #####\n\n");

    //-----CHIUSURA DELLE RISORSE-----

    //Terminazione dei thread workers e del signal handler
    close(listen_fd);
    for(int i=0;i<n_workers;i++){
        printf("Aspetto la terminazione del thread %d\n",i);
        pthread_join(threadpool[i],NULL);
    }
    printf("Aspetto la terminazione del signal handler thread\n");
    pthread_join(signal_handler_thread,NULL);
    //deallocazione del threadpool e della sua lista di lavoro
    free(threadpool);
    list_destroy(queue);

    
    printf("\n*****STATISTICHE DI CHIUSURA*****\n");
    //Stampa dei file presenti nello storage al momento della chiusura
    printf("Storage al momento della terminazione:\n");
    hash_dump(stdout,storage,print_stored_file_info);
    CHECKRETURNVALUE(ctrl,DestroyStorage(),"Errore distruggendo lo storage",;);
    
    max_data_num= (max_data_num == -1) ? 0 : max_data_num;
    max_data_size= (max_data_size == -1) ? 0 : max_data_size;
    printf("Numero massimo di file memorizzati:%d\nDimensione massima raggiunta:%d\nNumero attivazioni dell'algoritmo di rimpiazzamento %d\n",max_data_num,max_data_size,nr_of_replacements);
    LOGFILEAPPEND("STATISTICHE FINALI\nNumero massimo di file memorizzati:%d\nDimensione massima raggiunta:%d\nNumero attivazioni dell'algoritmo di rimpiazzamento %d\n",max_data_num,max_data_size,nr_of_replacements);
    LOGFILEAPPEND("Server spento!\n");
    //Chiudiamo il file di log
    if(fclose(logfile)!=0){
        perror("Errore in chiusura del file di log");
        return -1;
    }
    /*
    pthread_mutex_destroy(&mutex_storage);
    pthread_mutex_destroy(&mutex_logfile);
    pthread_mutex_destroy(&term_var);
    */
    return 0;    
}

void* WorkerFun(void* p){
    printf("[WORKER %ld] Ho iniziato la mia esecuzione!\n",pthread_self());
    int pipe_fd=*((int*)p);
    int condition=1;
    int ctrl;
    int op_return;
    while(condition){
        //Estraiamo un fd dalla coda
        printf("[WORKER %ld] aspetto di estrarre qualcuno dalla coda\n",pthread_self());
        int current_fd=list_pop(&queue);
        printf("[WORKER %ld] Ho estratto dalla coda il fd %d!\n",pthread_self(),current_fd);
        if(current_fd==-1){ //Se estraggo -1 dalla coda devo terminare forzatamente
            printf("[WORKER %ld] Ho estratto il fd %d perciò TERMINO\n",pthread_self(),current_fd);
            condition=0;
            pthread_cond_signal(&list_not_empty);
        }else{
            int operation;
            SYSCALL(ctrl,read(current_fd,&operation,4),"Errore nella 'read' dell'operazione");
            LOGFILEAPPEND("[WORKER %ld] Ho ricevuto la seguente richiesta: %d\n",pthread_self(),operation);
            printf("[WORKER %ld] Ho ricevuto la seguente richiesta: %d\n",pthread_self(),operation);
            op_return=ExecuteRequest(operation,current_fd);
            switch(op_return){
                case 0:
                    printf("[WORKER %ld] Operazione %d andata a buon fine\n",pthread_self(),operation);
                    break;
                case 1:
                    printf("[WORKER %ld] Operazione %d andata a buon fine, ESCO\n",pthread_self(),operation);
                    break;
                case -1:
                    printf("[WORKER %ld] Operazione %d fallita\n",pthread_self(),operation);
                    break;
            }
        }

        SYSCALL(ctrl,write(pipe_fd,&current_fd,4),"Errore nella 'write' del fd sulla pipe");
        if(!condition || op_return==1){ 
            //Se il client invia il messaggio di terminazione oppure devo forzatamente terminare
            int w=1;
            SYSCALL(ctrl,write(pipe_fd,&w,4),"Errore nella 'write' del flag sulla pipe");
        }else{
            int w=0;
            SYSCALL(ctrl,write(pipe_fd,&w,4),"Errore nella 'write' del flag sulla pipe");
        }
    }
    printf("[WORKER %ld] Sono terminato\n",pthread_self());
    pthread_mutex_lock(&term_var);
    terminated_workers++;
    printf("Ho variato terminated che ora e' %d\n",terminated_workers);
    pthread_mutex_unlock(&term_var);
    fflush(stdout);
    //pthread_exit((void*)0);
    pthread_attr_destroy(p);
    return NULL;
}

void* SignalHandlerFun(void* arg){
    sigset_t* set=(sigset_t*)arg;
    int ret;
    int condition=1;
    while(condition){
        int sig;
        CHECKRETURNVALUE(ret,sigwait(set,&sig),"Errore nella 'sigwait' in SignalHandlerFun",return NULL);

        switch (sig){
            case SIGINT:{
                printf("Ricevuto SIGINT -> Terminare il prima possibile\n");
                fflush(stdout);
                list_push_terminators(&queue,n_workers);
                forced_term=1;
                condition=0;
                break;
            }
            case SIGQUIT:{
                printf("Ricevuto SIGQUIT -> Terminare in modo graceful\n");
                fflush(stdout);
                pthread_mutex_lock(&term_var);
                graceful_term=1;
                pthread_mutex_unlock(&term_var);
                break;
            }
            case SIGHUP:{
                printf("PIPPO\n");
                break;
            }
            default:
                break;
        }
    }
    printf("Signal Handler Thread TERMINATO\n");
    fflush(stdout);
    pthread_attr_destroy(arg);
    return NULL;
}

void Welcome(){
    system("clear");
    printf(" ______ _____ _      ______    _____ _______ ____  _____            _____ ______ \n");
    printf("|  ____|_   _| |    |  ____|  / ____|__   __/ __ \\|  __ \\     /\\   / ____|  ____|\n");
    printf("| |__    | | | |    | |__    | (___    | | | |  | | |__) |   /  \\ | |  __| |__ \n");
    printf("|  __|   | | | |    |  __|    \\___ \\   | | | |  | |  _  /   / /\\ \\| | |_ |  __|\n");
    printf("| |     _| |_| |____| |____   ____) |  | | | |__| | | \\ \\  / ____ \\ |__| | |____\n");
    printf("|_|    |_____|______|______| |_____/   |_|  \\____/|_|  \\_\\/_/    \\_\\_____|______|\n");
    printf("***Server File Storage ATTIVO***\n\n");
}

