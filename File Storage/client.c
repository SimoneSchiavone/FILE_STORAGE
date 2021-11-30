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

#define BUFFDIM 256
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}
#define CALLOC(r) r=(char*) calloc(BUFFDIM,sizeof(char)); if(r==NULL){ perror("calloc"); return EXIT_FAILURE;}

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

int main(int argc,char** argv){
    error=0;
    backup_dir=NULL;
    int w_or_W_to_do=0;

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
        printf("ANALIZZO: %c\n",opt);
        switch (opt){
            case 'h':{
                //Stampa la lista di tutte le opzioni accettate dal client e termina immediatamente
                PrintAcceptedOptions();
                goto exit;
            }
            case 'f':{
                socketname=optarg;
                break;
            }
            case 'w':{
                //Invia al server i file della cartella dirname, se n=0 o non specificato non c'e' un
                //limite al nr di file altrimenti invia n file visitando ricorsivamente la cartella
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
                    error=1;
                    goto exit;
                }
                new->next=NULL;
                //Preparazione operazione
                new->op->op_code=3;
                new->op->argc=c;
                new->op->args=(char**)malloc(c*sizeof(char*)); //una o due stringhe

                list_push(command_list,new);
                break;
            }
            case 'W':{
                //Invia al server i file specificati
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

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                list_push(command_list,new);
                break;
            }
            case 'D':{
                if(!w_or_W_to_do){
                    fprintf(stderr,"E' stata specificata l'opzione -d senza aver prima specificato -w o -W!\n");
                    error=1;
                    goto exit;
                }
                backup_dir=optarg;
                break;
            }
            case 'r':{
            //Leggi dal server i file memorizzati
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

                char* file=strtok_r(optarg,",",&tmp);
                while(file){
                    new->op->args[i]=strndup(file,strlen(file));
                    i++;
                    file=strtok_r(NULL,",",&tmp);
                }

                list_push(command_list,new);
                break;
            }
            case 'R':{
                char* hey=optarg;
                printf("%s\n",hey);
                break;
            }
            case ':':{
                switch (optopt){
                    case 'R':{
                        printf("Argomento non presente, va bene così");
                        break;
                    }
                    default:{
                        fprintf(stderr, "option -%c is missing a required argument\n", optopt);
                    }
                }
                break;
            }
            case 'd':{
                read_dir=optarg;
                break;
            }
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

    //DEBUGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
    printf("Enableprinting:%d\n",enable_printing);
     
    struct timespec a;
    a.tv_sec=15;

    if(socketname==NULL){
        if(openConnection(SOCKNAME,5000,a)==-1)
            return EXIT_FAILURE;
    }else{
        if(openConnection(socketname,5000,a)==-1)
            return EXIT_FAILURE;
    }
    //char name[]="/Topolino\0";
    //printf("Dimensione: %ld Stringa: %s\n",strlen(name),name);

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

    /*
    sleep(5);
    printf("\n\n*****OPEN FILE MINNIE*****\n");
    openFile("minnie.txt\0",1,1);
    printf("\n\n*****WRITE FILE MINNIE*****\n");
    writeFile("minnie.txt\0","Espulsi");
    readFile("minnie.txt\0",NULL,NULL);
    */
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

    exit:
        list_destroy(command_list);
        free(command_list);
        if(!error)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
}
