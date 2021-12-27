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

#define SOCKNAME "./SocketFileStorage.sock"
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); exit(errno);}

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
//file_name* opened_files;

void Execute_Requests(operation_node* request_list);

int main(int argc,char** argv){
    error=0,delay=0,w_or_W_to_do=0,print_options=0;
    backup_dir=NULL;
    //opened_files=NULL;

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
    Welcome();

    operation_node* command_list=NULL;

    //Parsing degli argomenti da linea di comando
    int opt;
    while((opt=getopt(argc,argv,":phf:w:W:D:r:d:t:l:u:c:R:"))!=-1){
        switch (opt){
            case 'h':{ //Stampa opzioni accettate e termina
                PrintAcceptedOptions();
                goto exit;
            }
            case 'f':{ //Imposta nome del socket AF_UNIX
                if (optarg[0] == '-') {
						fprintf(stderr, "Il comando -f necessita di un argomento.\n");
						goto exit;
				}
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
                if (optarg[0] == '-') {
						fprintf(stderr, "Il comando -w necessita di un argomento.\n");
						goto exit;
				}
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
                if (optarg[0] == '-') {
						fprintf(stderr, "Il comando -W necessita di un argomento.\n");
						goto exit;
				}
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
                if (optarg[0] == '-') {
						fprintf(stderr, "Il comando -D necessita di un argomento.\n");
						goto exit;
				}
                if(!w_or_W_to_do){
                    fprintf(stderr,"E' stata specificata l'opzione -d senza aver prima specificato -w o -W!\n");
                    error=1;
                    goto exit;
                }
                backup_dir=optarg;
                break;
            }
            case 'r':{ //Leggi dal server i file specificati
                if (optarg[0] == '-') {
						fprintf(stderr, "Il comando -R necessita di un argomento.\n");
						goto exit;
				}
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
                if(optarg!=NULL){
                    IF_PRINT_ENABLED(printf("Caso R con argomento %s\n",optarg););
                    if (optarg[0] == '-') {
						printf("Ha preso come argomento il -\n");
                    }
                }
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
                new->op->argc=1;
                new->op->args=(char**)malloc(sizeof(char*)); 
                if(!new->op->args){
                    fprintf(stderr,"Errore nella malloc degli argomenti\n");
                    free(new);
                    free(new->op);
                    error=-1;
                    goto exit;
                }     
                new->op->args[0]=optarg;
                if(list_insert_operation_end(&command_list,new)==-1){
                    fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                    free(new);
                    free(new->op);
                    free(new->op->args);
                    goto exit;
                }     
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
            case 'p':{
                if(print_options==1){
                    fprintf(stderr,"-p non puo' essere ripetuto piu' volte\n");
                    error=1;
                    goto exit;
                }
                printf("Stampe ABILITATE\n");
                printf("----------\n");
                print_options=1;
                break;
            }
            case ':':{
                if(optopt=='R'){ //Caso R senza argomento
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
                    new->op->option=optopt;
                    new->op->argc=0;
                    new->op->args=NULL; 

                    if(list_insert_operation_end(&command_list,new)==-1){
                        fprintf(stderr,"Errore nell'inserimento dell'operazione della lista di esecuzione\n");
                        free(new);
                        free(new->op);
                        goto exit;
                    }
                    break;
                }else{
                    printf("Argomento mancante per l'opzione -%c!\n",optopt);
                    goto exit;
                }
            }
            case '?':{
                printf("Opzione -%c sconosciuta!\n",optopt);
                goto exit;
            }
        }
    }
    
    struct timespec a;
    a.tv_sec=15;
    print_command_list(command_list);
    if(socketname==NULL){
        if(openConnection(SOCKNAME,5000,a)==-1)
            goto exit;
    }else{
        if(openConnection(socketname,5000,a)==-1)
            goto exit;
    }

    print_command_list(command_list);
    Execute_Requests(command_list);
    
    if(socketname){
        if(closeConnection(socketname)==-1){
            IF_PRINT_ENABLED(fprintf(stderr,"Errore in chiusura di connessione con %s\n",socketname););
        }
    }else{
        if(closeConnection(SOCKNAME)==-1){
            IF_PRINT_ENABLED(fprintf(stderr,"Errore in chiusura di connessione con %s\n",SOCKNAME););
        }
    }
        
    exit:
        list_destroy(command_list);
        if(!error)
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
}

void Execute_Requests(operation_node* request_list){
    operation_node* curr=request_list;
    struct timespec ts;
    ts.tv_sec=delay/1000;
    ts.tv_nsec=(delay % 1000)*1000000;
    while(curr!=NULL){  
        nanosleep(&ts,NULL); //Ritardo artificiale tra le operazione

        //DEBUG
        IF_PRINT_ENABLED(printf("\n----------\nOp Estratta -> %c Argc %d Args ",curr->op->option,curr->op->argc););
        int w;
        for(w=0;w<curr->op->argc;w++){
            IF_PRINT_ENABLED(printf("%s ",curr->op->args[w]););
        }
        if(w==0)
            IF_PRINT_ENABLED(printf(" (None)\n"););
        IF_PRINT_ENABLED(printf("\n"););
        
        //Operazione di scrittura di una lista di files
        if(curr->op->option=='W'){
            for(int i=0;i<curr->op->argc;i++){ //Scorriamo i file da scrivere sul file storage
                int open_return=-1,write_return=-1;
                if((open_return=openFile(curr->op->args[i],1,1))==-1){ //Open con la create
                    open_return=openFile(curr->op->args[i],0,1); //Open con la lock
                }
                if(open_return==0){ //procediamo alla scrittura se una delle due aperture e' andata a buon fine
                    if((write_return=writeFile(curr->op->args[i],backup_dir))==-1){ //se la Write non e' andata a buon fine provo a fare una append
                        //Scansioniamo il contenuto del file da inviare e proviamo l'operazione append
                        FILE* to_append;
                        if((to_append=fopen(curr->op->args[i],"r"))!=NULL){
                            int ctrl;
                            struct stat st;
                            SYSCALL(ctrl,stat(curr->op->args[i], &st),"Errore nella 'stat'");
                            size_t size = (size_t)st.st_size;
                            int filedim=(int)size; 
                            char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
                            if(read_file!=NULL){
                                fread(read_file,sizeof(char),filedim,to_append);
                                read_file[filedim]='\0';
                            }   
                            fclose(to_append);
                            write_return=appendToFile(curr->op->args[i],read_file,size,backup_dir);
                        }
                    }     
                }

                //Stampa Esito
                if(open_return==-1){ //fallimento open
                    printf("Operazione -w su %s FALLITA\n",curr->op->args[i]);
                }else{
                    if(write_return==-1){ //fallimento write|append
                        printf("Operazione -w su %s FALLITA\n",curr->op->args[i]);
                    }
                    if(write_return==0){
                        if(closeFile(curr->op->args[i])==0)
                            printf("Operazione -w su %s COMPLETATA\n",curr->op->args[i]);
                        else
                            printf("Operazione -w su %s FALLITA\n",curr->op->args[i]);
                    }
                }
                printf("----------\n");
            }
        }

        //Operazione di scrittura di "n" files della cartella
        if(curr->op->option=='w'){ 
            file_name* to_send=NULL;
            int num;
            if(curr->op->argc==2){ //'n' specificato
                num=(int)strtol(curr->op->args[1],NULL,10);
                if(num<0){ //errore vado alla prossima operazione
                    fprintf(stderr,"Opzione -W con n<0 non consentita\n");
                    curr=curr->next;
                    continue;

                }
            }else{
                num=0;
            }
            //carico i nomi dei file da inviare nella lista to_send
            if(num==0){
                //invio tutti i file della directory
                if(files_in_directory(&to_send,curr->op->args[0])==-1){
                    fprintf(stderr,"Errore nella lettura dei file da inviare\n");
                }
            }else{
                //invio al piu' 'w' che sono nella directory
                if(n_files_in_directory(&to_send,curr->op->args[0],w)==-1){
                    fprintf(stderr,"Errore nella lettura dei file da inviare");
                }
            }
            //invio i file caricati nella lista al server
            file_name* f=to_send;
            while(f){
                int open_return=-1,write_return=-1;
                if((open_return=openFile(f->name,1,1))==-1){ //Open con la create
                    open_return=openFile(f->name,0,1); //Open con la lock
                }
                if(open_return==0){ //procediamo alla scrittura se una delle due aperture e' andata a buon fine
                    if((write_return=writeFile(f->name,backup_dir))==-1){ //se la Write non e' andata a buon fine provo a fare una append
                        //Scansioniamo il contenuto del file da inviare e proviamo l'operazione append
                        FILE* to_append;
                        if((to_append=fopen(f->name,"r"))!=NULL){
                            int ctrl;
                            struct stat st;
                            SYSCALL(ctrl,stat(f->name, &st),"Errore nella 'stat'");
                            size_t size = (size_t)st.st_size;
                            int filedim=(int)size; 
                            char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
                            if(read_file!=NULL){
                                fread(read_file,sizeof(char),filedim,to_append);
                                read_file[filedim]='\0';
                            }   
                            fclose(to_append);
                            write_return=appendToFile(f->name,read_file,size,backup_dir);
                        }
                    }     
                }
                //Stampa Esito
                if(open_return==-1){ //fallimento open
                    printf("Operazione -w su %s FALLITA\n",f->name);
                }else{
                    if(write_return==-1){ //fallimento write|append
                        printf("Operazione -w su %s FALLITA\n",f->name);
                    }
                    if(write_return==0){
                        if(closeFile(f->name)==0)
                            printf("Operazione -w su %s COMPLETATA\n",f->name);
                        else
                            printf("Operazione -w su %s FALLITA\n",f->name);
                    }
                }
                f=f->next;
            }    
            free_name_list(to_send);
        }

        //Operazione di lettura di una lista di files
        if(curr->op->option=='r'){
            for(int i=0;i<curr->op->argc;i++){
                int read_return=-1;
                int open_return=openFile(curr->op->args[i],0,0);
                if(open_return==0){
                    char* buf;
                    size_t s;
                    read_return=readFile(curr->op->args[i],(void**)&buf,&s);
                    if(buf!=NULL)
                        free(buf);
                }

                //Stampa Esito
                if(open_return==-1){ //fallimento open
                    printf("Operazione -r su %s FALLITA\n",curr->op->args[i]);
                }else{
                    if(read_return==-1){ //fallimento write|append
                        printf("Operazione -r su %s FALLITA\n",curr->op->args[i]);
                    }else{
                        if(read_return==0){
                            if(closeFile(curr->op->args[i])==0)
                                printf("Operazione -r su %s COMPLETATA\n",curr->op->args[i]);                
                            else
                                printf("Operazione -r su %s FALLITA\n",curr->op->args[i]); 
                        }
                    }
                }
                printf("----------\n");
            }
        }

        //Operazione di lettura di 'n' files dallo storage
        if(curr->op->option=='R'){
            int read_n_return=-1;
            if(curr->op->argc==0){
                read_n_return=readNFiles(0,read_dir);
                //Stampa Esito
                if(read_n_return==0)
                    printf("Operazione -R di tutto lo storage FALLITA\n");
                else
                    printf("Operazione -R di tutto lo storage COMPLETATA\n");
            }else{
                int num=(int)strtol(curr->op->args[0],NULL,10);

                read_n_return=readNFiles(num,read_dir);
                //Stampa Esito
                if(read_n_return==0)
                    printf("Operazione -R %d FALLITA\n",num);
                else
                    printf("Operazione -R %d COMPLETATA\n",num);
            }

        }

        //Operazione di lock di una lista di files
        if(curr->op->option=='l'){
            for(int i=0;i<curr->op->argc;i++){
                int open_return=openFile(curr->op->args[i],0,0);
                int lock_return=-1;
                if(open_return==0){
                    lock_return=lockFile(curr->op->args[i]);
                }

                //Stampa Esito
                if(open_return==-1){
                    printf("Operazione -l su %s FALLITA\n",curr->op->args[i]);
                }else{
                    if(lock_return==0){
                        if(closeFile(curr->op->args[i])==0){
                            printf("Operazione -l su %s COMPLETATA\n",curr->op->args[i]);
                        }else{
                            printf("Operazione -l su %s FALLITA\n",curr->op->args[i]);
                        }
                    }else{
                        printf("Operazione -l su %s FALLITA\n",curr->op->args[i]);
                    }
                }
                printf("----------\n");
            }
        }

        //Operazione di unlock di una lista di files
        if(curr->op->option=='u'){
            for(int i=0;i<curr->op->argc;i++){
                int open_return=openFile(curr->op->args[i],0,0);
                int unlock_return=-1;
                if(open_return==0){
                    unlock_return=unlockFile(curr->op->args[i]);
                }

                //Stampa Esito
                if(open_return==-1){
                    printf("Operazione -u su %s FALLITA\n",curr->op->args[i]);
                }else{
                    if(unlock_return==0){
                        if(closeFile(curr->op->args[i])==0){
                            printf("Operazione -u su %s COMPLETATA\n",curr->op->args[i]);
                        }else{
                            printf("Operazione -u su %s FALLITA\n",curr->op->args[i]);
                        }
                    }else{
                        printf("Operazione -u su %s FALLITA\n",curr->op->args[i]);
                    }
                }
                printf("----------\n");
            }
        }

        //Operazione di cancellazione di una lista di files
        if(curr->op->option=='c'){
            for(int i=0;i<curr->op->argc;i++){
                int open_return=openFile(curr->op->args[i],0,0);
                int cancel_return=-1;
                if(open_return==0){
                    cancel_return=removeFile(curr->op->args[i]);
                }

                //Stampa Esito
                if(open_return==-1){
                    printf("Operazione -c su %s FALLITA\n",curr->op->args[i]);
                }else{
                    if(cancel_return==0){
                        printf("Operazione -c su %s COMPLETATA\n",curr->op->args[i]);
                    }else{
                        printf("Operazione -c su %s FALLITA\n",curr->op->args[i]);
                    }
                }
                printf("----------\n");
            }
        }

        curr=curr->next;
    }
}