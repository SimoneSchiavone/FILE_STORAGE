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

typedef struct signal_handler_thread_arg{
        int pipe;
        sigset_t* set;
}signal_handler_thread_arg;

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
    int signal_to_server[2];
    int tmp;
    SYSCALL(tmp,pipe(signal_to_server),"Errore nella creazione della pipe");
    pthread_t signal_handler_thread;
    signal_handler_thread_arg arg;
    arg.pipe=signal_to_server[1];
    arg.set=&mask;
    CHECKRETURNVALUE(ctrl,pthread_create(&signal_handler_thread,NULL,SignalHandlerFun,&arg),"Errore nella creazione del thread signal handler",goto exit);



    //-----Creazione del socket e setting dell'indirizzo-----
    int listen_fd; //file descriptor socket
    int connection_fd;
    SYSCALL(listen_fd,socket(AF_UNIX,SOCK_STREAM,0),"Errore nella 'socket'");
    struct sockaddr_un sa;
    memset(&sa, '0', sizeof(sa));
    strncpy(sa.sun_path,socket_name,UNIX_PATH_MAX);
    sa.sun_family=AF_UNIX;

    SYSCALL(ctrl,bind(listen_fd,(struct sockaddr*)&sa,sizeof(sa)),"Errore nella 'bind'");
    SYSCALL(ctrl,listen(listen_fd,max_connections_bound),"Errore nella 'listen'");

    //-----Creazione threadpool-----
    threadpool=(pthread_t*)malloc(n_workers*sizeof(pthread_t));
    queue=NULL;
    if(threadpool==NULL){
        perror("Errore nella 'malloc' del threadpool");
        goto exit;
    }

    //-----Creazione Pipe-----
    int wtm_pipe[2]; //WorkerToMaster_Pipe utilizzata per la comunicazione dal thread worker verso il server/manager
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
    MAX_FD(signal_to_server[0]);
    FD_SET(wtm_pipe[0],&set);
    FD_SET(signal_to_server[0],&set);

    active_connections=0;
    max_active_connections=0;

    int graceful_term=0; 

    while(1){
        printf("[MAIN] Server in attesa ...\n");
        //Copio il set nella variabile per la select
        rdset=set;
    
        if( (active_connections==0) & graceful_term){
            printf("[MAIN] Tutti i client sono stati serviti, chiusura del server\n");
            if(concurrent_list_push_terminators(&queue,n_workers)==-1){
                printf("Errore nell'inserimento dei terminatori\n");
            }
            break;
        }


        if(select(fd_max+1,&rdset,NULL,NULL,NULL)==-1){
            if(errno==EINTR){
                printf("Interrotta la select");
            }else
                perror("Errore nella 'select'");
            goto exit;
        }

        //Cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
        for(int i=0;i<=fd_max;i++){
            if(FD_ISSET(i,&rdset)){ 
                if(i==listen_fd){ //e' una nuova richiesta di connessione! 
                    SYSCALL(connection_fd,accept(listen_fd,(struct sockaddr*)NULL,0),"Errore nella 'accept'");
                    if(graceful_term){ //non posso accettare ulteriori connessioni
                        int ok=0;
                        SYSCALL(ctrl,write(connection_fd,&ok,sizeof(int)),"Errore in scrittura del bit di accettazione");
                        printf("[Server_MAIN] Respinta una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
                        LOGFILEAPPEND("[MAIN] Respinta una connessione sul fd %d\n",connection_fd);
                    }else{
                        int ok=1;
                        SYSCALL(ctrl,write(connection_fd,&ok,sizeof(int)),"Errore in scrittura del bit di accettazione");
                        active_connections++;
                        max_active_connections= (max_active_connections<active_connections) ? active_connections : max_active_connections;
                        printf("[Server_MAIN] Accettata una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
                        LOGFILEAPPEND("[MAIN] Accettata una connessione sul fd %d\n",connection_fd);
                        FD_SET(connection_fd,&set);
                        MAX_FD(connection_fd);
                    }
                }else if (i==wtm_pipe[0]){ //e' la pipe di comunicazione Worker to Manager che passa l'id del fd da reinserire in lista
                    int received_fd;
                    int byte_read;
                    int finished;
                    SYSCALL(byte_read,read(wtm_pipe[0],&received_fd,sizeof(int)),"Errore nella 'read' del fd restituito dal worker");
                    SYSCALL(byte_read,read(wtm_pipe[0],&finished,sizeof(int)),"Errore nella 'read' del flag di terminazione client proveniente dal worker");
                    if(finished){
                        if(received_fd!=-1 && received_fd!=1){
                            printf("[MAIN] Il client sul fd %d ha eseguito una operazione ed è TERMINATO\n",received_fd);
                            FD_CLR(received_fd,&set);
                            Search_New_Max_FD(set,fd_max);
                            int e;
                            SYSCALL(e,close(received_fd),"Errore nella chiusura del fd restituito dal worker");
                            active_connections--;
                            printf("[MAIN] Chiusa la connessione sul fd %d - Connessioni attive %d\n",received_fd,active_connections);
                        }
                    }else{
                        printf("[MAIN] Il client sul fd %d ha eseguito una operazione e non è terminato\n",received_fd);
                        FD_SET(received_fd,&set);
                        MAX_FD(received_fd);
                    }
                }else if(i==signal_to_server[0]){ //e' la pipe di comunicazione con il signal handler thread
                    int stop;
                    SYSCALL(ctrl,read(signal_to_server[0],&stop,sizeof(int)),"Errore nella 'read' del fd restituito dal worker");
                    if(stop==1){ //Terminazione immediata
                        printf("[Server_Main] Ho ricevuto un segnale di terminazione immediata, esco\n");
                        LOGFILEAPPEND("[MAIN] Ho ricevuto un segnale di terminazione immediata, esco");
                        goto exit;
                    }
                    if(stop==2){ //Terminazione graceful
                        graceful_term=1;
                        printf("[Server_Main] Ho ricevuto un segnale di terminazione graceful\n");
                        LOGFILEAPPEND("[MAIN] Ho ricevuto un segnale di terminazione graceful\n");
                        FD_CLR(signal_to_server[0],&set);
                        Search_New_Max_FD(set,fd_max);
                    }
                }else{ //e' un client pronto in lettura
                    printf("[MAIN] Il client sul fd %d è pronto in lettura\n",i);
                    if(concurrent_list_push(&queue,i)==-1){
                        printf("Errore nella 'list_push' di un fd");
                        goto exit;
                    }
                    printf("[MAIN] Il client sul fd %d è stato inserito in coda\n",i);
                    FD_CLR(i,&set);
                }
            }
        }
     
    }

    close(wtm_pipe[0]);
    close(signal_to_server[0]);
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

    
    
    //Stampa dei file presenti nello storage al momento della chiusura
    printf("Storage al momento della terminazione:\n");
    hash_dump(stdout,storage,print_stored_file_info);
    CHECKRETURNVALUE(ctrl,DestroyStorage(),"Errore distruggendo lo storage",;);
    printf("Ho distrutto lo storage ed i file rimasti!\n");
    
    printf("\n*****STATISTICHE DI CHIUSURA*****\n");
    max_data_num= (max_data_num == -1) ? 0 : max_data_num;
    max_data_size= (max_data_size == -1) ? 0 : max_data_size;
    printf("Numero massimo di file memorizzati:%d\nDimensione massima raggiunta:%d\nNumero attivazioni dell'algoritmo di rimpiazzamento %d\nNumero massimo di connessioni contemporanee:%d\n",max_data_num,max_data_size,nr_of_replacements,max_active_connections);
    LOGFILEAPPEND("STATISTICHE FINALI\nNumero massimo di file memorizzati:%d\nDimensione massima raggiunta:%d\nNumero attivazioni dell'algoritmo di rimpiazzamento:%d\nNumero massimo di connessioni contemporanee:%d\n",max_data_num,max_data_size,nr_of_replacements,max_active_connections);
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
        //printf("[WORKER %ld] aspetto di estrarre qualcuno dalla coda\n",pthread_self());
        int current_fd=concurrent_list_pop(&queue);
        //printf("[WORKER %ld] Ho estratto dalla coda il fd %d!\n",pthread_self(),current_fd);
        if(current_fd==-1){ //Se estraggo il file descriptor -1 dalla coda allora devo terminare il thread
            printf("[WORKER %ld] Ho estratto il fd %d perciò TERMINO\n",pthread_self(),current_fd);
            condition=0;
            //Faccio risvegliare i thread che stanno aspettando fd nella coda perche' il server non inserira' nessun altro
            pthread_cond_signal(&list_not_empty);
        }else{
            int operation;
            SYSCALL(ctrl,read(current_fd,&operation,sizeof(int)),"Errore nella 'read' dell'operazione");
            LOGFILEAPPEND("[WORKER-%ld]\nHo ricevuto la seguente richiesta:%d\n",pthread_self(),operation);
            printf("[WORKER %ld] Ho ricevuto la seguente richiesta: %d\n",pthread_self(),operation);
            op_return=ExecuteRequest(operation,current_fd);
            switch(op_return){
                case 0:
                    printf("[WORKER %ld] Operazione %d andata a buon fine\n\n\n",pthread_self(),operation);
                    break;
                case 1:
                    printf("[WORKER %ld] Operazione %d andata a buon fine, ESCO\n\n\n",pthread_self(),operation);
                    break;
                case -1:
                    printf("[WORKER %ld] Operazione %d fallita\n\n",pthread_self(),operation);
                    break;
            }

            SYSCALL(ctrl,write(pipe_fd,&current_fd,sizeof(int)),"Errore nella 'write' del fd sulla pipe");
            if(!condition || op_return==1){ 
                //Se il client invia il messaggio di terminazione oppure devo forzatamente terminare
                int w=1;
                SYSCALL(ctrl,write(pipe_fd,&w,sizeof(int)),"Errore nella 'write' del flag sulla pipe");
            }else{
                int w=0;
                SYSCALL(ctrl,write(pipe_fd,&w,sizeof(int)),"Errore nella 'write' del flag sulla pipe");
            }
        }
    }
    
    fflush(stdout);
    pthread_attr_destroy(p);
    printf("[WORKER %ld] Sono terminato\n",pthread_self());
    return NULL;
}

void* SignalHandlerFun(void* arg){
    signal_handler_thread_arg* a=(signal_handler_thread_arg*)arg;
    sigset_t* set=a->set;

    int ret;
    int condition=1;
    while(condition){
        int sig;
        CHECKRETURNVALUE(ret,sigwait(set,&sig),"Errore nella 'sigwait' in SignalHandlerFun",return NULL);

        switch (sig){
            //case SIGQUIT:
            case SIGINT:{
                printf("Ricevuto SIGINT -> Terminare il prima possibile\n");
                fflush(stdout);
                int received_a_signal=1;
                SYSCALL(ret,write(a->pipe,&received_a_signal,sizeof(int)),"Errore nella 'write' del fd sulla pipe");
                printf("SignalHandlerThread-> Ho inviato la segnalazione al server tramite pipe\n");
                if(concurrent_list_push_terminators(&queue,n_workers)==-1){
                    printf("[SignalHandler] Errore nell'inserimento dei terminatori\n");
                }
                condition=0;
                break;
            }
            case SIGQUIT:
            case SIGHUP:{
                printf("Ricevuto SIGQUIT -> Terminare in modo graceful\n");
                fflush(stdout);
                int received_a_signal=2;
                SYSCALL(ret,write(a->pipe,&received_a_signal,sizeof(int)),"Errore nella 'write' del fd sulla pipe");
                printf("SignalHandlerThread-> Ho inviato la segnalazione al server tramite pipe\n");
                condition=0;
                break;
            }
            default:
                break;
        }
    }
    close(a->pipe);
    printf("Signal Handler Thread TERMINATO\n");
    fflush(stdout);
    //pthread_attr_destroy(arg);
    return NULL;
}

void Welcome(){
    printf(" ______ _____ _      ______    _____ _______ ____  _____            _____ ______ \n");
    printf("|  ____|_   _| |    |  ____|  / ____|__   __/ __ \\|  __ \\     /\\   / ____|  ____|\n");
    printf("| |__    | | | |    | |__    | (___    | | | |  | | |__) |   /  \\ | |  __| |__ \n");
    printf("|  __|   | | | |    |  __|    \\___ \\   | | | |  | |  _  /   / /\\ \\| | |_ |  __|\n");
    printf("| |     _| |_| |____| |____   ____) |  | | | |__| | | \\ \\  / ____ \\ |__| | |____\n");
    printf("|_|    |_____|______|______| |_____/   |_|  \\____/|_|  \\_\\/_/    \\_\\_____|______|\n");
    printf("***Server File Storage ATTIVO***\n\n");
}

