// created by Simone Schiavone at 20211007 21:52.
// @Università di Pisa
// Matricola 582418

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serverutils.h"
#include <errno.h>
#include <stdarg.h>

void PrintConfiguration(){
    printf("NWorkers: %d   MaxFilesNum: %d   MaxFilesDim: %d   SocketName: %s   Logfilename: %s\n",n_workers,max_files_num,max_files_dim,socket_name,logfilename);
}

/*Funzione per la creazione del file di log. Restituisce 0 se creato correttamente, -1 in caso di errore*/
int LogFileCreator(char* name){
    //Controllo parametri
    if(name==NULL)
        return -1;

    //Se esiste già un file di log con questo nome rimuoviamolo
    unlink(name);

    //Apriamo il file
    if((logfile=fopen(name,"a"))==NULL){
        perror("Errore nell'apertura del file di log");
        return -1;
    }

    //Segnaliamo nel file di log la creazione del file avvenuta con successo
    char* str="Creazione del file di log avvenuta con successo!\n";
    if(fwrite(str,sizeof(char),strlen(str),logfile)<0){
        perror("Errore nella scrittura del file");
        return -1;
    }

    /*
    //Chiudiamo il file di log
    if(fclose(logfile)!=0){
        perror("Errore in chiusura del file di log");
        return -1;
    }*/
    return 0;
}

/*ScanConfiguration effettua la lettura del file di configurazione, il cui path è passato dall'utente,
per configurare i parametri del server sopra riportati. La funzione restituisce 0 se l'operazione
è andata a buon fine, 1 in caso di errore. In caso di parole chiave non riconosciute oppure righe
con formato scorretto la procedura di lettura termina con un insuccesso ed il server procede con
i settaggi di default*/

int ScanConfiguration(char* path){
    //Controllo parametri
    if(path==NULL){
        return -1;
    }

    //Apro il file di configurazione in modalità lettura
    FILE* configfile=fopen(config_file_path,"r");
    if(configfile==NULL){
        perror("Errore durante l'apertura del file di configurazione");
        exit(errno);
    }

    char line[128];
    memset(line,'\0',128);
    //Leggiamo il file
    while(fgets(line,128,configfile)!=NULL){
        printf("%s",line);
        
        //tokenizzazione della stringa
        char *tmpstr;
        char *token = strtok_r(line," \n", &tmpstr); //token conterrà la keyword del parametro da settare

        if(strncmp(token,"NWORKERS",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            n_workers=atoi(token);
        }
        if(strncmp(token,"MAX_FILE_N",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            max_files_num=atoi(token);
        }
        if(strncmp(token,"MAX_FILE_DIM",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            max_files_dim=atoi(token);
        }
        if(strncmp(token,"SOCKET_FILE_PATH",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            memset(socket_name,'\0',128);
            strncpy(socket_name,token,strlen(token));
        }
        if(strncmp(token,"LOG_FILE_NAME",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            memset(logfilename,'\0',128);
            strncpy(logfilename,token,strlen(token));
        }

        if((token=strtok_r(NULL," \n",&tmpstr))!=NULL){
            //Se entro qui significa che in tale linea ho più due stringhe es: NWORKERS 5 x y ...
            fprintf(stderr,"!!! Formato del file di configurazione errato - Partenza con parametri di default !!!\n");
            DefaultConfiguration();
            break;
        }
        memset(line,'\0',128);
    }

    if(LogFileCreator(logfilename)==-1){
        perror("Errore nella creazione del file di log");
        return -1;
    }

    //Chiusura del file
    if(fclose(configfile)!=0){
        perror("Fclose configfile");
        exit(errno);
    }

    return EXIT_SUCCESS;
}

/*La procedura effettua il ripristino dei parametri di configurazione del server */
void DefaultConfiguration(){
    strncpy(config_file_path,"./txt/config_file.txt",22);
    n_workers=5; // numero thread workers del modello Manager-Worker
    max_files_num=100;  //numero massimo di files
    max_files_dim=128;  //dimensione massima dei files in MBytes
    memset(socket_name,'\0',128);
    strncpy(socket_name,"./SocketFileStorage",20); //nome del socket AF_UNIX
    memset(logfilename,'\0',128);
    strncpy(logfilename,"./txt/logfile.txt",18); //nome del socket AF_UNIX
    logfile=NULL;
    pthread_mutex_init(&mutex_logfile,NULL);
    pthread_mutex_init(&mutex_list,NULL);
    pthread_mutex_init(&mutex_storage,NULL);
    pthread_cond_init(&list_not_empty,NULL);
    max_connections=5;
    storage=NULL;

}

/*Funzione per la scrittura di una stringa nel file di log. La funzione restituisce 0 se è andato
tutto a buon fine, -1 altrimenti*/
int LogFileAppend(char* format,...){
    if(format==NULL)
        return -1;
    fprintf(logfile,"--------------------\n");
    va_list list; //lista degli argomenti della funzione
    va_start(list,format);
    pthread_mutex_lock(&mutex_logfile);
    vfprintf(logfile,format,list);
    /* The  functions  vprintf(), vfprintf(), vdprintf(), vsprintf(), vsnprintf() are equivalent
    to the functions printf(), fprintf(), dprintf(), sprintf(), snprintf(), respectively, except
    that they are called with a va_list instead of a variable number of arguments. These functions
    do not call the va_end macro. Because they invoke the va_arg macro, the value of ap is undefined
    after the call.  See stdarg(3). All of these functions write the output under the control of a format string that specifies how  subsequent  arguments
    (or arguments accessed via the variable-length argument facilities of stdarg(3)) are converted for output.
    */
    pthread_mutex_unlock(&mutex_logfile);
    va_end(list);
    return 0;
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

int InitializeStorage(){
    if((storage= hash_create(15,hash_pjw,string_compare))==NULL)
        return -1;
    return 0;
}

int DestroyStorage(){
    return hash_destroy(storage,NULL,free); //il campo freekey è null perchè la stringa non è allocata sullo heap
}

int ExecuteRequest(int fun,int fd){
    switch(fun){
        case (3):{
            int intbuffer; //Buffer di interi
            int ctrl; //variabile che memorizza il risultato di ritorno di chiamate di sistema/funzioni

            //Leggo la dimensione del pathname e poi leggo il pathname
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST] Errore nella 'read' della dimensione del pathname");
            printf("[EXECUTE REQUEST-OpenFile] Devo ricevere un path di dimensione %d\n",intbuffer);
            char* stringbuffer; //buffer per contenere la stringa
            if(!(stringbuffer=(char*)calloc(intbuffer,sizeof(char)))){ //Verifica malloc
                perror("[EXECUTE REQUEST-OpenFile] Errore nella 'malloc' del buffer per pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,stringbuffer,intbuffer),"[EXECUTE REQUEST-OpenFile] Errore nella 'read' del pathname");
            printf("[EXECUTE REQUEST-OpenFile] Ho ricevuto il path %s di dimensione %d\n",stringbuffer,ctrl);
            
            //Leggo il flag o_create
            int o_create;
            SYSCALL(ctrl,read(fd,&o_create,4),"[EXECUTE REQUEST-OpenFile] Errore nella 'read' del flag o_create");
            if(o_create)
                printf("[EXECUTE REQUEST-OpenFile] Ho ricevuto il flag OCREATE\n");

            //Leggo il flag o_lock
            int o_lock;
            SYSCALL(ctrl,read(fd,&o_lock,4),"[EXECUTE REQUEST-OpenFile] Errore nella 'read' del flag o_locK");
            if(o_lock)
                printf("[EXECUTE REQUEST-OpenFile] Ho ricevuto il flag o_lock\n");

            //Chiamata alla funzione OpenFile che restituisce un oggetto di tipo response che incapsula
            //al suo interno un messaggio ed un codice di errore
            response r=OpenFile(stringbuffer,o_create,o_lock,fd);
            free(stringbuffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST-OpenFile] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST-OpenFile] Errore nella 'write' della risposta");

            return r.code;
        }
        case (12):{ //fine comando
            return 1;
        }
        default:{
            printf("Comando sconosciuto");
            return -1;
        }
    }
    return 0;
}

/*Richiesta di apertura o creazione di un file.*/
response OpenFile(char* pathname, int o_create,int o_lock,int fd_owner){
    response r;
    printf("[FILE STORAGE-OpenFile] Il path ricevuto è %s\n",pathname);
    char* id=strndup(pathname,strlen(pathname));
    if(o_create){ //Il file va creato
        stored_file* new_file=(stored_file*)malloc(sizeof(stored_file));
        if(new_file==NULL){
            errno=ENOMEM;
            perror("Errore nella creazione del nuovo file");
            r.code=-1;
            sprintf(r.message,"Errore nella creazione del nuovo file\n");
        }
        new_file->content=NULL;
        if(o_lock){
            new_file->fd_owner=fd_owner;
        }else{
            new_file->fd_owner=-1;
        }
        switch(hash_insert(storage,id,new_file)){
            case 1: //null parameters
                errno=EINVAL;
                perror("[FILE STORAGE] Parametri errati");
                free(new_file);
                r.code=-1;
                sprintf(r.message,"Parametri openFileErrati");
                break;
            case 2: //key già presente
                errno=EEXIST;
                perror("[FILE STORAGE] Il file è già presente nello storage\n");
                free(new_file);
                r.code=-1;
                sprintf(r.message,"Il file %s è già presente nello storage!",id);
                break;
            case 3: //errore malloc nuovo nodo
                errno=ENOMEM;
                perror("Malloc nuova entry hash table");
                free(new_file);
                r.code=-1;
                sprintf(r.message,"Errore nella malloc del nuovo file");
                break;
            case 0:
                printf("[FILE STORAGE] %s inserito nello storage\n",id);
                hash_dump(stdout,storage,print_stored_file_info);
                r.code=0;
                sprintf(r.message,"OK Il file %s è stato inserito nello storage",id);;
        }
    }else{ //Il file deve essere presente
        if(hash_find(storage,pathname)==NULL){
            errno=ENOENT;
            perror("[FILE STORAGE] Il file non è presente nello storage\n");
            r.code=-1;
            sprintf(r.message,"Il file %s NON è presente nello storage\n",id);
        }
    }
    return r;
}

void print_stored_file_info(FILE* stream,void* file){
    stored_file* param=(stored_file*)file;
    fprintf(stream,"Size of content:%d Owner:%d\n",(int)sizeof(param->content),param->fd_owner);
}