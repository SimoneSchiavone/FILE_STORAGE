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
#include <dirent.h>
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
file_name* opened_files;

void Execute_Requests(operation_node* request_list);

int main(int argc,char** argv){
    error=0,delay=0,w_or_W_to_do=0,print_options=0;
    backup_dir=NULL;
    opened_files=NULL;

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
    //Welcome();

    operation_node* command_list=NULL;

    //Parsing degli argomenti da linea di comando
    int opt;
    while((opt=getopt(argc,argv,"hf:w:W:D:r:R:d:t:l:u:c:p"))!=-1){
        switch (opt){
            case 'h':{ //Stampa opzioni accettate e termina
                PrintAcceptedOptions();
                goto exit;
                /*
                if(print_options){
                    fprintf(stderr,"L'opzione -h non puo' essere ripetuto\n");
                    goto exit;
                }else{
                    print_options=1;
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
                    free(new);
                    error=1;
                    goto exit;
                }
                //Preparazione operazione
                new->op->option=0;
                new->op->argc=0;
                new->op->args=NULL;
            
                if(list_insert_operation_start(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    goto exit;
                }
                break;*/
            }
            case 'f':{ //Imposta nome del socket AF_UNIX
                if(socketname==NULL){
                    socketname=optarg;
                    break;
                }else{
                    fprintf(stderr,"Hai gia' impostato il nome del socket (%s)\n",socketname);
                    error=1;
                    goto exit;
                }
            }
            case 'w':{ //Invia al server 'n' file della cartella dirname
                char* tmp;
                int c=1; //argc
                char* directory=strtok_r(optarg,",",&tmp); //directory che contiene i file da inviare
                if(!directory){
                    fprintf(stderr,"Errore opzione -w: nome directory necessario");
                    error=1;
                    goto exit;
                }
                printf("Ho trovato:%s\n",directory);
                char* number=strtok_r(NULL,",",&tmp); //nr di file da inviare (argomento opzionale)
                if(number){
                    c++;
                    if(strtok_r(NULL,",",&tmp)){
                        fprintf(stderr,"Errore opzione -w: troppi argomenti");
                        error=-1;
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
                new->op->option=opt;
                new->op->argc=c;
                new->op->args=(char**)malloc(c*sizeof(char*)); //una o due stringhe
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    error=-1;
                    free(new);
                    free(new->op);
                    goto exit;
                }
                new->op->args[1]=strdup(number);
                new->op->args[0]=strdup(directory);

                
                w_or_W_to_do=1;

                //Inserimento in fondo alla lista dei comandi
                if(list_insert_operation_end(&command_list,new)==-1){
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
                //conto le virgole della stringa per definire quanti argomenti sono stati passati
                int dim=Count_Commas(optarg)+1; 

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                new->next=NULL;

                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->op->option=opt;
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
                int i=0;
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                w_or_W_to_do=1;

                if(list_insert_operation_end(&command_list,new)==-1){
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
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                new->next=NULL;

                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->op->option=opt;
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
                int i=0;
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_operation_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }
                break;
            }
            case 'R':{ //Leggi 'n' file qualsiasi dal server, se n non e' specificato leggi tutti i file
                //TODO: Da implementare
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
                delay=(int)strtol(optarg,NULL,10);
                break;
            }
            case 'l':{
                char* tmp;
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                new->next=NULL;

                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->op->option=opt;
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
                int i=0;
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_operation_end(&command_list,new)==-1){
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
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                new->next=NULL;

                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione                
                new->op->option=opt;
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
                int i=0;
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_operation_end(&command_list,new)==-1){
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
                int dim=Count_Commas(optarg)+1;

                //Nuovo nodo della lista  di comandi
                operation_node* new=(operation_node*)malloc(sizeof(operation_node));
                if(!new){
                    perror("Malloc new node");
                    error=1;
                    goto exit;
                }
                new->next=NULL;

                //Nuova operazione
                new->op=(pending_operation*)malloc(sizeof(pending_operation));
                if(!new->op){
                    perror("Malloc new operation");
                    free(new);
                    error=1;
                    goto exit;
                }
                
                //Preparazione operazione
                new->op->option=6;
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
                int i=0;
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                if(list_insert_operation_end(&command_list,new)==-1){
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
    Execute_Requests(command_list);
    goto exit;
    //printf("Enableprinting:%d\n",enable_printing);
    
    struct timespec a;
    a.tv_sec=15;

    if(socketname==NULL){
        if(openConnection(SOCKNAME,5000,a)==-1)
            goto exit;
    }else{
        if(openConnection(socketname,5000,a)==-1)
            goto exit;
    }

    printf("\n-----OPENFILE GREENPASS.PDF-----\n");
    openFile("File di prova/greenpass.pdf\0",1,1);
    sleep(1);
    printf("\n-----WRITEFILE GREENPASS.PDF-----\n");
    writeFile("File di prova/greenpass.pdf\0","Espulsi");
    sleep(1);
    printf("\n-----LEGGO -1 FILE-----\n");
    read_dir="File Letti\0";
    readNFiles(-1,"File Letti");
    sleep(1);
    printf("\n-----READ FILE GREENPASS.PDF-----\n");
    readFile("File di prova/greenpass.pdf\0",NULL,NULL);
    sleep(1);
    closeConnection(socketname);

    exit:
        list_destroy(command_list);
        if(!error)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
}

void Execute_Requests(operation_node* request_list){
    operation_node* curr=request_list;
    while(curr!=NULL){
        //Ritardo artificiale tra le operazione
        sleep(delay);

        //Operazione del nodo corrente
        if(curr->op->option=='W'){ //Operazione di scrittura di una lista di file
            for(int i=0;i<curr->op->args;i++){ //Scorriamo i file da scrivere sul file storage
                int ok_open=1;
                if(is_file_name_in_list(opened_files,curr->op->args[i])){ //il file non e' gia' stato aperto
                    if(openFile(curr->op->args[i],1,1)!=0){
                        fprintf(stderr,"Errore nella OpenFile\n");
                        ok_open=0;
                    }
                }

                //WriteFile con cartella di backup se disponibile
                if(ok_open){
                    if(writeFile(curr->op->args[i],backup_dir)!=0){
                        fprintf(stderr,"Errore nella writeFile\n");
                    }
                }
            }
        }


        if(curr->op->option='w'){ //Operazione di scrittura di "n" file della cartella
            file_name* to_send=NULL;
            if(curr->op->args==2){ //caso 'n' specificato
                if(curr->op->args[1]<0){
                    fprintf(stderr,"Opzione -W con n<0 non consentita\n");
                }else{
                    //carico i nomi dei file da inviare
                    if(curr->op->args[1]==0){
                        if(files_in_directory(&to_send,curr->op->args[0])==-1){
                            fprintf(stderr,"Errore nella lettura dei file da inviare\n");
                        }
                    }else{
                        if(n_files_in_directory(&to_send,curr->op->args[0],curr->op->args[1])==-1){
                            fprintf(stderr,"Errore nella lettura dei file da inviare");
                        }
                    }
                    //invio i file al server
                    file_name* f=to_send;
                    while(f){
                        if(writeFile(f->name,backup_dir)==-1){
                            fprintf(stderr,"Errore nella WriteFile di %s\n",f->name);
                        }
                        f=f->next;
                    }
                }
            }
        }

        
    }
}