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
#include <signal.h>

#include "client_utils/serverAPI.h"
#include "client_utils/clientutils.h"

#define SOCKNAME "./SocketFileStorage"
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}
#define IF_PRINT_ENABLED(print) if(print_options){print}

/*
static void signal_handler(int signum){
    switch (signum){
        case SIGPIPE: {
            write(1,"Errore fatale di comunicazione con il server",45);
            abort();
        }
        default:{
            abort();
            break;
        }
    }
}*/
int error;
int delay;
int print_options;

int main(int argc,char** argv){
    error=0,delay=0,w_or_W_to_do=0,print_options=0;
    backup_dir=NULL;

    int ctrl;
    struct sigaction s;
    memset(&s, 0, sizeof(s));
    //s.sa_handler = signal_handler;
    s.sa_handler=SIG_IGN;
    sigset_t set;
    SYSCALL(ctrl,sigfillset(&set), "Errore nella sigfillset");
    SYSCALL(ctrl,pthread_sigmask(SIG_SETMASK, &set, NULL), "Errore nella pthread_sigmask");
    SYSCALL(ctrl,sigaction(SIGPIPE, &s, NULL), "Errore sigaction SIGINT"); //gestore per SIGPIPE
    SYSCALL(ctrl,sigemptyset(&set), "Errore in sigemptyset");
    SYSCALL(ctrl,pthread_sigmask(SIG_SETMASK,&set,NULL),"Errore in sigmask rimozione maschera");

    char* socketname=NULL;
    int enable_printing=0;
    Welcome();

    operation_node* command_list=NULL;

    //Parsing degli argomenti da linea di comando
    int opt;
    while((opt=getopt(argc,argv,"hf:w:W:D:r:R:d:t:l:u:c:p"))!=-1){
        switch (opt){
            case 'h':{ //Stampa opzioni accettate e termina
                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                //Preparazione operazione
                new->op->op_code=0;
                new->op->argc=0;
                new->op->args=NULL;
                /*
                PrintAcceptedOptions();
                goto exit;*/
                if(list_insert_start(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    goto exit;
                }
                break;
            }
            case 'f':{ //Imposta nome del socket AF_UNIX
                if(socketname==NULL)
                    socketname=optarg;
                else{
                    fprintf(stderr,"Hai gia' impostato il nome del socket (%s)\n",socketname);
                    error=1;
                    goto exit;
                }
                break;
            }
            case 'w':{ //Invia al server 'n' file della cartella dirname
                char* tmp;
                int c=1;
                char* directory=strtok_r(optarg,",",&tmp);
                if(!directory){
                    fprintf(stderr,"Errore opzione -w: nome directory necessario");
                    error=1;
                    goto exit;
                }else
                    printf("Ho parsato %s\n",directory);
                char* number=strtok_r(NULL,",",&tmp);
                if(number){
                    if(strtok_r(NULL,",",&tmp)){
                        fprintf(stderr,"Errore opzione -w: troppi argomenti");
                        error=-1;
                        c++;
                        goto exit;
                    }
                }
                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new->op);
                    error=1;
                    goto exit;
                }
                new->next=NULL;
                //Preparazione operazione
                new->op->op_code=3;
                new->op->argc=1;
                new->op->args=(char**)malloc(sizeof(char*)); //una o due stringhe
                new->op->args[0]=strndup(number,strlen(number));
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    error=-1;
                    free(new);
                    free(new->op);
                    goto exit;
                }

                
                w_or_W_to_do=1;
                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'W':{ //Invia al server i file specificati
                char* tmp;
                int i=0;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->next=NULL;
                new->op->op_code=4;
                new->op->argc=dim;
                new->op->args=(char**)malloc(dim*sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                w_or_W_to_do=1;

                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'D':{ //Specifica la cartella dove memorizzare i file espulsi per capacity misses
                if(!w_or_W_to_do){
                    fprintf(stderr,"E' stata specificata l'opzione -d senza aver prima specificato -w o -W!\n");
                    error=1;
                    goto exit;
                }
                backup_dir=optarg;
                break;
            }
            case 'r':{ //Leggi dal server i file specificati
                char* tmp;
                int i=0;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->next=NULL;
                new->op->op_code=6;
                new->op->argc=dim;
                new->op->args=(char**)malloc(dim*sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'R':{ //Leggi 'n' file qualsiasi dal server, se n non e' specificato leggi tutti i file
                printf("Caso R con argomento\n");
                printf("%s\n",optarg);
                int n;
                sscanf(optarg, "%d", &n);
                    printf("%d\n",n);
                                        
                break;
            }
            case 'd':{ //Specifica la cartella dove memorizzare i file letti dallo storage
                read_dir=optarg;
                break;
            }
            case 't':{ //Tempo che intercorre tra l'invio di due richieste successive
                delay=atoi(optarg);
                break;
            }
            case 'l':{
                char* tmp;
                int i=0;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->next=NULL;
                new->op->op_code=4;
                new->op->argc=dim;
                new->op->args=(char**)malloc(dim*sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'u':{
                char* tmp;
                int i=0;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->next=NULL;
                new->op->op_code=4;
                new->op->argc=dim;
                new->op->args=(char**)malloc(dim*sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'c':{
                char* tmp;
                int i=0;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->next=NULL;
                new->op->op_code=4;
                new->op->argc=dim;
                new->op->args=(char**)malloc(dim*sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'p':{
                if(enable_printing==1){
                    fprintf(stderr,"-p non puo' essere ripetuto piu' volte\n");
                    error=1;
                    goto exit;
                }
                enable_printing=1;
                break;
            }
            case ':':{
                if(optopt=='R')
                    printf("Caso R senza argomento\n");
            }
            case '?':{
                printf("Necessario argomento\n");
                break;
            }
            default:{
                printf("opzione sconosciuta\n");
                break;
            }
        }
    }

    print_command_list(command_list);

    //DEBUGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
    printf("Enableprinting:%d\n",enable_printing);
     
    struct timespec a;
    a.tv_sec=15;

    if(socketname==NULL){
        if(openConnection(SOCKNAME,5000,a)==-1)
            goto exit;
    }else{
        if(openConnection(socketname,5000,a)==-1)
            goto exit;
    }

    printf("\n\n*****OPEN FILE TOPOLINO*****\n");
    openFile("topolino.txt\0",1,1);

    sleep(1);

    printf("\n\n*****WRITE FILE TOPOLINO*****\n");
    writeFile("topolino.txt\0","Espulsi");

    sleep(1);

    printf("\n\n*****READ FILE TOPOLINO*****\n");
    readFile("topolino.txt\0",NULL,NULL);

    sleep(1);

    printf("\n\n*****APPEND TO FILE TOPOLINO*****\n");
    char* to_append="CIAONE\0";    
    appendToFile("topolino.txt\0",to_append,strlen(to_append)+1,"Espulsi");

    sleep(1);

    printf("\n\n*****READ FILE TOPOLINO*****\n");
    readFile("topolino.txt\0",NULL,NULL);

    sleep(1);

    printf("\n\n*****UNLOCK FILE TOPOLINO*****\n");
    unlockFile("topolino.txt\0");

    sleep(1);

    printf("\n\n*****REMOVE FILE TOPOLINO*****\n");
    removeFile("topolino.txt\0");

    sleep(1);

    printf("\n\n*****LOCK FILE TOPOLINO*****\n");
    lockFile("topolino.txt\0");

    sleep(1);
    
    printf("\n\n*****REMOVE FILE TOPOLINO*****\n");
    removeFile("topolino.txt\0");

    closeConnection(socketname);

    exit:
        list_destroy(command_list);
        if(!error)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
}
