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
#define SYSCALL_VOID_(r,c,e) if((r=c)==-1) {perror(e); printf("#ERRORE#\n"); continue;}
#define CHECKRETURNVALUE(r,command,e,t) if((r=command)!=0) {perror(e); t;}
#define MAX_FD(a) if(a>fd_max) fd_max=a;

/*Struct che incapsula gli argomenti da passare al thread gestore dei segnali cioe'
la maschera ed il pipe di comunicazione per l'invio di un intero di notifica al server*/
typedef struct signal_handler_thread_arg{
        int pipe;
        sigset_t* set;
}signal_handler_thread_arg;

int Search_New_Max_FD(fd_set set,int maxfd);
void reset_socket();
void* WorkerFun(void* p);
void* SignalHandlerFun(void* arg);
void Welcome();

int main(){     
    int ctrl;
    
    //-----Messaggio di benvenuto-----
    Welcome();

    //-----Configurazione del server-----
    printf("[Server_Main] Default Configuration: ");
    DefaultConfiguration();
    PrintConfiguration();
    if(ScanConfiguration(config_file_path)==-1){
        printf("[Server_Main] Errore fatale nella scansione del file di configurazione\n");
        return -1;
    }
    printf("[Server_Main] Starting Configuration: ");
    PrintConfiguration();

    //-----Inizializzazione dello Storage
    CHECKRETURNVALUE(ctrl,InitializeStorage(),"Errore inizializzando lo storage",return -1);

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
    CHECKRETURNVALUE(ctrl,pthread_sigmask(SIG_BLOCK,&mask,NULL),"Errore in 'pthread_sigmask",return -1);
    
    struct sigaction s;
    memset(&s,0,sizeof(s));
    s.sa_handler=SIG_IGN; //ignora SIGPIPE
    CHECKRETURNVALUE(ctrl,sigaction(SIGPIPE,&s,NULL),"Errore in 'sigaction",return -1);
    int signal_to_server[2];
    int tmp;
    SYSCALL(tmp,pipe(signal_to_server),"Errore nella creazione della pipe server-signal_handler");
    pthread_t signal_handler_thread;
    signal_handler_thread_arg arg; //struct argomento della signal handler thread function
    arg.pipe=signal_to_server[1]; //pipe di segnalazione
    arg.set=&mask;
    CHECKRETURNVALUE(ctrl,pthread_create(&signal_handler_thread,NULL,SignalHandlerFun,&arg),"Errore nella creazione del thread signal handler",return -1);

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
    queue=NULL; //coda di file descriptor
    if(threadpool==NULL){
        perror("Errore nella 'malloc' del threadpool");
        return -1;
    }

    //-----Creazione Pipe-----
    int wtm_pipe[2]; //WorkerToMaster_Pipe utilizzata per la comunicazione dal thread worker verso il server/manager
    SYSCALL(tmp,pipe(wtm_pipe),"Errore nella creazione della pipe");

    //-----Creazione Thread Workers-----
    pthread_t t;
    for(int i=0;i<n_workers;i++){
            CHECKRETURNVALUE(ctrl,pthread_create(&t,NULL,WorkerFun,(void*)&wtm_pipe[1]),"Errore nella creazione del thread",goto exit);
            threadpool[i]=t;
    }

    int fd_max=0;
    MAX_FD(listen_fd); // tengo traccia del file descriptor con id piu' grande
    MAX_FD(wtm_pipe[0]);
    MAX_FD(signal_to_server[0]);

    //-----Registrazione del welcome socket e delle pipe
    fd_set set,rdset;
    FD_ZERO(&set);
    FD_SET(listen_fd,&set);
    FD_SET(wtm_pipe[0],&set);
    FD_SET(signal_to_server[0],&set);

    int active_connections=0; //numero di connessioni attive
    int connections_number=0;
    int max_active_connections=0; //numero massimo di connessioni contemporanee attive
    int graceful_term=0; //terminazione graceful dopo SIGHUP

    while(1){
        printf("[Server_Main] Server in attesa ...\n");
        //Copio il set nella variabile per la select
        rdset=set;

        //Se non ci sono piu' connessioni attive ed ho ricevuto SIGHUP, termina
        if( (active_connections==0) & graceful_term){
            printf("[Server_Main] Tutti i client sono stati serviti, chiusura del server\n");
            //inserisco nella lista di lavoro tanti '-1' quanti sono i thread lavoratori in modo da farli terminare
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
        
        //Determiniamo da quale fd abbiamo ricevuto una richiesta
        for(int i=0;i<=fd_max;i++){
            if(FD_ISSET(i,&rdset)){ 
                if(i==listen_fd){ //e' una nuova richiesta di connessione
                    SYSCALL(connection_fd,accept(listen_fd,(struct sockaddr*)NULL,0),"Errore nella 'accept'");

                    if(graceful_term){ 
                        //non posso accettare ulteriori connessioni, respingo inviando al client un bit di conferma=0
                        int ok=0;
                        SYSCALL(ctrl,writen(connection_fd,&ok,sizeof(int)),"Errore in scrittura del bit di accettazione");
                        printf("[Server_Main] Respinta una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
                        LOGFILEAPPEND("[MAIN] Respinta una connessione sul fd %d\n",connection_fd);
                    }else{ 
                        //posso accettare ulteriori connessioni, respingo inviando al client un bit di conferma=1
                        int ok=1;
                        SYSCALL(ctrl,writen(connection_fd,&ok,sizeof(int)),"Errore in scrittura del bit di accettazione");
                        active_connections++;
                        connections_number++;
                        max_active_connections= (max_active_connections<active_connections) ? active_connections : max_active_connections;
                        printf("[Server_Main] Accettata una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
                        LOGFILEAPPEND("[MAIN] Accettata una connessione sul fd %d - Connessioni attive %d\n",connection_fd,active_connections);
                        FD_SET(connection_fd,&set);
                        MAX_FD(connection_fd);
                    }

                }else if (i==wtm_pipe[0]){ //e' la pipe di comunicazione Worker to Manager che passa l'id del fd da reinserire in lista
                    int received_fd;
                    int byte_read;
                    int finished;
                    //leggo il fd restituito dal thread worker
                    SYSCALL(byte_read,readn(wtm_pipe[0],&received_fd,sizeof(int)),"Errore nella 'read' del fd restituito dal worker");
                    //leggo il bit di terminazione restituito dal thread worker
                    SYSCALL(byte_read,readn(wtm_pipe[0],&finished,sizeof(int)),"Errore nella 'read' del flag di terminazione client proveniente dal worker");
                    //il client ha terminato le operazioni
                    if(finished){
                        printf("[Server_Main] Il client sul fd %d ha eseguito una operazione ed e' TERMINATO\n",received_fd);
                        FD_CLR(received_fd,&set);
                        if(received_fd==fd_max)
                            fd_max=Search_New_Max_FD(set,fd_max);
                        printf("Il nuovo massimo e' %d\n",fd_max);
                        int e;
                        SYSCALL(e,close(received_fd),"Errore nella chiusura del fd restituito dal worker");
                        active_connections--;
                        printf("[Server_Main] Chiusa la connessione sul fd %d - Connessioni attive %d\n",received_fd,active_connections);
                        LOGFILEAPPEND("[MAIN] Chiusa la connessione sul fd %d\n",received_fd);

                    }else{
                        printf("[Server_Main] Il client sul fd %d ha eseguito una operazione e non e' terminato\n",received_fd);
                        FD_SET(received_fd,&set);
                        MAX_FD(received_fd);
                    }

                }else if(i==signal_to_server[0]){ //e' la pipe di comunicazione con il signal handler thread
                    int stop;
                    SYSCALL(ctrl,readn(signal_to_server[0],&stop,sizeof(int)),"Errore nella 'read' del fd restituito dal worker");
                    if(stop==1){ //Terminazione immediata
                        printf("[Server_Main] Ho ricevuto un segnale di terminazione immediata, esco\n");
                        LOGFILEAPPEND("[MAIN] Ho ricevuto un segnale di terminazione immediata, esco");
                        goto exit;
                    }
                    if(stop==2){ //Terminazione graceful (continuo a servire i client gia' connessi)
                        graceful_term=1;
                        printf("[Server_Main] Ho ricevuto un segnale di terminazione graceful\n");
                        LOGFILEAPPEND("[MAIN] Ho ricevuto un segnale di terminazione graceful\n");
                        FD_CLR(signal_to_server[0],&set);
                        Search_New_Max_FD(set,fd_max);
                    }
                }else{ //e' un client pronto
                    printf("[Server_Main] Il client sul fd %d e' pronto in lettura\n",i);
                    //inserimento nella lista di lavoro del fd pronto
                    if(concurrent_list_push(&queue,i)==-1){
                        printf("Errore nella 'list_push' di un fd");
                        goto exit;
                    }
                    printf("[Server_Main] Il client sul fd %d è stato inserito in coda\n",i);
                    FD_CLR(i,&set);
                }
            }
        }
     
    }

    
    exit:
        printf("\n##### Procedura di uscita #####\n\n");

    //-----CHIUSURA DELLE RISORSE-----
    close(wtm_pipe[0]);
    close(signal_to_server[0]);

    //Terminazione dei thread workers e del signal handler
    close(listen_fd);
    for(int i=0;i<n_workers;i++){
        //printf("Aspetto la terminazione del thread %d\n",i);
        pthread_join(threadpool[i],NULL);
    }
    //printf("Aspetto la terminazione del signal handler thread\n");
    pthread_join(signal_handler_thread,NULL);

    //deallocazione del threadpool e della sua lista di lavoro
    free(threadpool);
    list_destroy(queue);

    //Stampa dei file presenti nello storage al momento della chiusura
    printf("[Server_Main] Storage al momento della terminazione:\n");
    //funzione stampa di una tabella hash
    hash_dump(stdout,storage,print_stored_file_info);
    //deallocazione file e storage
    CHECKRETURNVALUE(ctrl,DestroyStorage(),"Errore distruggendo lo storage",;);
    printf("[Server_Main] Ho distrutto lo storage ed i file rimasti!\n");
    
    //Statistiche di chiusura
    printf("\n*****STATISTICHE DI CHIUSURA*****\n");
    max_data_num= (max_data_num == -1) ? 0 : max_data_num;
    max_data_size= (max_data_size == -1) ? 0 : max_data_size;
    float MBmax=((float)max_data_size)/1000000;
    printf("Numero massimo di file memorizzati: %d\nDimensione massima raggiunta (in MB): %.8f\nNumero attivazioni dell'algoritmo di rimpiazzamento: %d\nNumero di connessioni accettate %d\nNumero massimo di connessioni contemporanee: %d\n",max_data_num,MBmax,nr_of_replacements,connections_number,max_active_connections);
    LOGFILEAPPEND("STATISTICHE FINALI\nNumero massimo di file memorizzati: %d\nDimensione massima raggiunta (in MB): %.8f\nNumero attivazioni dell'algoritmo di rimpiazzamento: %d\nNumero massimo di connessioni contemporanee: %d\n",max_data_num,MBmax,nr_of_replacements,max_active_connections);
    LOGFILEAPPEND("Server spento!\n");

    //Chiusura file di log
    if(fclose(logfile)!=0){
        perror("Errore in chiusura del file di log");
        return -1;
    }

    return 0;    
}

/*Procedura che viene eseguita da ogni thread worker del thread pool:
il worker preleva un fd di un client pronto ad eseguire una certa operazione da una lista
opportunamente sincronizzata; l'operazione richiesta e' rappresentata da un numero intero che viene
opportunamente interpretato dalla funzione ExecuteRequest. Dopo l'esecuzione dell'operazione il 
worker invia al server tramite la pipe di comunicazione il file descriptor servito ed un bit
che indica il termine delle operazioni per quel client (che quindi non sara' piu' inserito
nella lista di lavoro). Se il worker estrae un file descriptor '-1', inserito nella coda di
lavoro dal signal handler thread, termina la sua esecuzione.*/

void* WorkerFun(void* p){
    printf("[WORKER %ld] Ho iniziato la mia esecuzione!\n",pthread_self());
    //Pipe di comunicazione worker to master
    int pipe_fd=*((int*)p);
    int condition=1;
    int ctrl;
    int op_return; //codice di ritorno dell'operazione eseguita

    while(condition){
        //Estraiamo un fd dalla coda
        int current_fd=concurrent_list_pop(&queue);

        if(current_fd==-1){ //Se estraggo il file descriptor -1 dalla coda allora devo terminare il thread
            condition=0;
            //Faccio risvegliare i thread che stanno aspettando fd nella coda perche' il server non inserira' nessun altro
            pthread_cond_signal(&list_not_empty);
        }else{
            //lettura del codice operazione ricevuto dal client
            int operation; 
            printf("Lettura dell'operazione sul fd %d\n",current_fd);
            SYSCALL_VOID_(ctrl,readn(current_fd,&operation,sizeof(int)),"Errore nella 'read' dell'operazione");
            LOGFILEAPPEND("[WORKER-%ld]\nHo ricevuto la seguente richiesta:%d\n",pthread_self(),operation);

            //esecuzione dell'operazione richiesta
            op_return=ExecuteRequest(operation,current_fd); 
            //valutazione esito operazione
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

            //invio al server il fd servito
            SYSCALL_VOID_(ctrl,writen(pipe_fd,&current_fd,sizeof(int)),"Errore nella 'write' del fd sulla pipe");
            if(op_return==1){ //il client ha terminato le operazioni 
                int w=1;
                SYSCALL_VOID_(ctrl,writen(pipe_fd,&w,sizeof(int)),"Errore nella 'write' del flag sulla pipe");
            }else{ //il client non ha terminato le operazioni
                int w=0;
                SYSCALL_VOID_(ctrl,writen(pipe_fd,&w,sizeof(int)),"Errore nella 'write' del flag sulla pipe");
            }
        }
    }
    
    fflush(stdout);
    printf("[WORKER %ld] Sono terminato\n",pthread_self());
    return NULL;
}

/*Procedura che viene eseguita dal signal handler thread. In caso di ricezione di un segnale
di terminazione immediata, cioe' sigint o sigquit, il thread invia al server tramite una pipe
di comunicazione un bit '1' e termina immediatamente. In caso di ricezione di un segnale di 
terminazione graceful il thread invia al server tramite una pipe di comunicazione un bit '2' e 
termina immediatamente*/
void* SignalHandlerFun(void* arg){
    signal_handler_thread_arg* a=(signal_handler_thread_arg*)arg;
    sigset_t* set=a->set;

    int ret;
    int condition=1;
    while(condition){
        int sig;
        CHECKRETURNVALUE(ret,sigwait(set,&sig),"Errore nella 'sigwait' in SignalHandlerFun",return NULL);

        switch (sig){
            case SIGQUIT:
            case SIGINT:{
                printf("Ricevuto SIGINT|SIGQUIT -> Terminare il prima possibile\n");
                fflush(stdout);
                //notifico il server della ricezione di un segnale di terminazione immediata
                int received_a_signal=1;
                SYSCALL_VOID_(ret,writen(a->pipe,&received_a_signal,sizeof(int)),"Errore nella 'write' del flag segnale sulla pipe");
                printf("SignalHandlerThread-> Ho inviato la segnalazione al server tramite pipe\n");
                //inserimento in lista di tanti '-1' quanti sono i workers in modo da farli terminare
                if(concurrent_list_push_terminators(&queue,n_workers)==-1){
                    printf("[SignalHandler] Errore nell'inserimento dei terminatori\n");
                }
                condition=0;
                break;
            }
            case SIGHUP:{
                printf("Ricevuto SIGHUP -> Terminare in modo graceful\n");
                fflush(stdout);
                //notifico il server della ricezione di un segnale di terminazione graceful
                int received_a_signal=2;
                SYSCALL_VOID_(ret,writen(a->pipe,&received_a_signal,sizeof(int)),"Errore nella 'write' del flag segnale sulla pipe");
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
    return NULL;
}

//Funzione di ricerca del fd massimo
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

//Procedura stampa di benvenuto
void Welcome(){
    printf(" ______ _____ _      ______    _____ _______ ____  _____            _____ ______ \n");
    printf("|  ____|_   _| |    |  ____|  / ____|__   __/ __ \\|  __ \\     /\\   / ____|  ____|\n");
    printf("| |__    | | | |    | |__    | (___    | | | |  | | |__) |   /  \\ | |  __| |__ \n");
    printf("|  __|   | | | |    |  __|    \\___ \\   | | | |  | |  _  /   / /\\ \\| | |_ |  __|\n");
    printf("| |     _| |_| |____| |____   ____) |  | | | |__| | | \\ \\  / ____ \\ |__| | |____\n");
    printf("|_|    |_____|______|______| |_____/   |_|  \\____/|_|  \\_\\/_/    \\_\\_____|______|\n");
    printf("***Server File Storage ATTIVO***\n\n");
}

