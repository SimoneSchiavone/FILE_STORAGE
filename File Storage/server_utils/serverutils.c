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
            //TESTINGGGGGGGGGGg
            max_files_dim=1100;
            //max_files_dim=atoi(token)*(1000000);/
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
        if(strncmp(token,"REPLACEMENT_POLICY",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            if(strncmp(token,"fifo",4)==0){
                replacement_policy=0;
            }else{ //Errato valore per la politica di rimpiazzamento
                fprintf(stderr,"!!! Politica di rimpiazzamento sconosciuta - Partenza con parametri di default\n");
                DefaultConfiguration();
                break;
            }
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
    max_files_dim=2048;  //dimensione massima dei files in MBytes
    replacement_policy=0; //politica di rimpiazzamento FIFO di default
    memset(socket_name,'\0',128);
    strncpy(socket_name,"./SocketFileStorage",20); //nome del socket AF_UNIX
    memset(logfilename,'\0',128);
    strncpy(logfilename,"./txt/logfile.txt",18); //nome del socket AF_UNIX
    logfile=NULL;
    if(pthread_mutex_init(&mutex_logfile,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return;
    }
    if(pthread_mutex_init(&mutex_list,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return;
    }
    if(pthread_mutex_init(&mutex_storage,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return;
    }
    if(pthread_cond_init(&list_not_empty,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return;
    }
    max_connections=5;
    storage=NULL;
}

/*Funzione per la scrittura di una stringa nel file di log. La funzione restituisce 0 se è andato
tutto a buon fine, -1 altrimenti*/
int LogFileAppend(char* format,...){
    time_t now=time(NULL);
    if(format==NULL)
        return -1;
    fprintf(logfile,"--------------------\n");
    fprintf(logfile,"%s",ctime(&now));
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

void free_stored_file(void* tf){
    stored_file* to_free=(stored_file*)tf;
    if(to_free!=NULL){
        if(to_free->content!=NULL)
            free(to_free->content);
        free(to_free);
    }
}

int DestroyStorage(){
    return hash_destroy(storage,free,free_stored_file);
}

int FIFO_Replacement(){
    pthread_mutex_lock(&mutex_storage);
    if(!storage || storage->nentries==0){
        pthread_mutex_unlock(&mutex_storage);
        return -1;
    }

    char* victim=NULL; //key della vittima
    time_t oldest_time=__INT_MAX__; //tempo di crezione della vittima
    int oldest_size=0; //dimensione della vittima
    
    entry_t *bucket,*curr_e;
    for(int i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr_e=bucket; curr_e!=NULL;) {
            if(curr_e->key){
                if ((curr_e->data)){
                    //stored_file da analizzare
                    printf("TIMESTAMP %s - %d\n",(char*)(curr_e->key),(int)(((stored_file*)(curr_e->data))->creation_time));
                    if((((stored_file*)(curr_e->data))->creation_time)<oldest_time){
                        //Nuova vittima individuata
                        victim=curr_e->key;
                        oldest_time=(((stored_file*)(curr_e->data))->creation_time);
                        oldest_size=((int)((stored_file*)(curr_e->data))->size);
                    }
                }
            }
            curr_e=curr_e->next;
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    if(hash_delete(storage,victim,NULL,free_stored_file)==-1){
        perror("Errore nell'eliminazione del file dallo storage");
        return -1;
    }else{
        //Aggiorno data_size e nr_entries
        pthread_mutex_lock(&mutex_storage);
        data_size-=oldest_size;
        LOGFILEAPPEND("Vittima designata->%s Dimensione->%d Creata il->%s Correttamente rimossa dallo storage \n",victim,oldest_size,ctime(&oldest_time));
        LOGFILEAPPEND("Occupazione dello storage dopo l'espulsione %d su %d bytes | Nr file memorizzati %d su %d",data_size,max_files_dim,storage->nentries,max_files_num);
        pthread_mutex_unlock(&mutex_storage);
        free(victim);
    }
    return 0;
}
int StartReplacementAlgorithm(){
    //Decidiamo quale politica di rimpiazzamento attuare
    int r=0;
    switch(replacement_policy){
        case 0:
            //Caso FIFO
            LOGFILEAPPEND("Avvio algoritmo di rimpiazzamento FIFO\n");
            if(FIFO_Replacement()==-1){
                return -1;
            }
            break;
        default:
            fprintf(stderr,"Politica di rimpiazzamento sconosciuta");
            return -1;
    }
    return r;
}

int StorageUpdateSize(hash_t* ht,int s){
    int r=0;
    int need_replacement=0;
    pthread_mutex_lock(&mutex_storage);
    need_replacement = (data_size+s<=max_files_dim) ? 0 : 1;
    pthread_mutex_unlock(&mutex_storage);

    if(!need_replacement){ //Ho spazio a sufficienza
        printf("HO SPAZIO A SUFFICIENZA PER MEMORIZZARE\n");
        data_size+=s;
        if(LogFileAppend("Occupazione dello storage: %d bytes su %d\n",data_size,max_files_dim)==-1){
            perror("Errore nella scrittura del file di log\n");
        }
    }else{ 
        printf("--NON-- HO SPAZIO A SUFFICIENZA PER MEMORIZZARE\n");
        //Devo attuare una politica di rimpiazzamento
        if(StartReplacementAlgorithm()!=0){
            printf("ALGORITMO DI RIMPIAZZAMENTO FALLITO");
            return -1;
        }else{
            //Dopo aver rimpiazzato un file verifico che non sia necessario espellere ulteriori file
            printf("Rimpiazzamento avvenuto con successo\n");
            StorageUpdateSize(ht,s);
        }
    }
    return r;
}

int CanWeStoreAnotherFile(){
    int r;
    pthread_mutex_lock(&mutex_storage);
    if(storage->nentries==max_files_num)
        r=0;
    else
        r=1;
    pthread_mutex_unlock(&mutex_storage);
    return r;
}

/*Richiesta di apertura o creazione di un file.*/
response OpenFile(char* pathname, int o_create,int o_lock,int fd_owner){
    response r;
    printf("[FILE STORAGE-OpenFile] Il path ricevuto è %s\n",pathname);
    char* id=strndup(pathname,strlen(pathname));

    if(o_create){ //Il file va creato
        //Verifichiamo che sia possibile creare un nuovo file nello storage (condizione sul nr max di file)
        if(!CanWeStoreAnotherFile()){
            r.code=-1;
            sprintf(r.message,"Non e' possibile memorizzare ulteriori file nello storage (max numero file archiviati raggiunto)\n");
            free(id);
            return r; 
        }
        stored_file* new_file=(stored_file*)malloc(sizeof(stored_file));
        if(new_file==NULL){
            errno=ENOMEM;
            perror("Errore nella creazione del nuovo file");
            r.code=-1;
            sprintf(r.message,"Errore nella creazione del nuovo file\n");
        }
        new_file->content=NULL; //Il contenuto è inizialmente nullo
        if(o_lock){ //Setto il proprietario
            new_file->fd_owner=fd_owner;
        }else{
            new_file->fd_owner=-1;
        }
        new_file->size=0; //Dimensione inizialmente nulla
        new_file->creation_time=time(NULL);
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
                sprintf(r.message,"OK Il file %s è stato inserito nello storage",id);
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

response WriteFile(char* pathname,char* content,int size){
    response r; 
    if(!pathname){
        r.code=-1;
        sprintf(r.message,"Il path e' nullo\n");
        return r;
    }

    //Verifico se il file esiste nello storage
    stored_file* file=hash_find(storage,pathname);
    if(!file){
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
        if(LogFileAppend("Il file %s non e' stato trovato",pathname)==-1){
            fprintf(stderr,"Errore nella scrittura del file di log\n");
        }
        //Libero la memoria
        free(pathname);
        free(content);
        return r;
    }

    //Verifico se lo storage può contenere il file
    if(size>max_files_dim){
        //Inserisco questo caso in modo che se un file ha una dimensione maggiore di quella
        //massima dello storage non si buttino via tutti i file per far spazio ad uno che
        //e' comunque troppo grande
        r.code=-1;
        sprintf(r.message,"La dimensione del file eccede la dimensione massima dello storage!");
        free(pathname);
        free(content);
        return r;
    }
    if(StorageUpdateSize(storage,size)==-1){ //Errore nella politica di rimpiazzamento
        r.code=-1;
        sprintf(r.message,"Errore nell'attuazione della politica di rimpiazzamento");
        free(pathname);
        free(content);
        return r;
    }
    
    file->content=content;
    file->size=(size_t)size;
    r.code=0;
    sprintf(r.message,"Il contenuto del file %s e' stato scritto sullo storage",pathname);
    if(LogFileAppend("Sono stati scritti %d bytes nel file con pathname %s\n",size,pathname)==-1){
        r.code=-1;
        sprintf(r.message,"Errore nella scrittura del file di log");
        return r;
    }

    hash_dump(stdout,storage,print_stored_file_info);
    free(pathname);
    return r;
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
        case (6):{
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 6] Errore nella 'read' della dimensione del pathname");
            char* read_name=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(read_name==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_name,intbuffer),"[EXECUTE REQUEST Case 6] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",read_name);

            //-----Lettura del file-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 6] Errore nella 'read' della dimensione del file da leggere");
            printf("Dimensione file %d\n",intbuffer);
            char* read_file=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del file
            if(read_file==NULL){
                perror("Errore nella malloc del file ricevuto");
                free(read_name); //prima di uscire dealloco il pathname gia' letto
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_file,intbuffer),"[EXECUTE REQUEST] Errore nella 'read' del file da leggere");
            printf("Ho ricevuto %d bytes\n",ctrl);
            printf("Ho ricevuto: %s\n",read_file);
            
            //-----Esecuzione operazione
            response r=WriteFile(read_name,read_file,intbuffer);

            //-----Invio risposta-----
            int responsedim=strlen(r.message);
            SYSCALL(ctrl,write(fd,&responsedim,sizeof(int)),"Errore nella 'write' della dimensione della risposta");
            printf("Ho inviato %d bytes di dimensione cioe' %d\n",ctrl,responsedim);
            SYSCALL(ctrl,write(fd,r.message,responsedim),"Errore nella 'write' della risposta");
            printf("Ho inviato %d bytes di stringa: %s\n",ctrl,r.message);
            return r.code;

            //int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            //SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST-OpenFile] Errore nella 'write' della dimensione della risposta");
            //SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST-OpenFile] Errore nella 'write' della risposta");
            //return r.code;
        }
        default:{
            printf("Comando %d sconosciuto!!!\n",fun);
            int ctrl;
            response r;
            r.code=-1;
            sprintf(r.message,"Comando %d sconosciuto!!!",fun);
            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST] Errore nella 'write' della risposta");
            return r.code;
        }
    }
    return 0;
}

void print_stored_file_info(FILE* stream,void* file){
    stored_file* param=(stored_file*)file;
    fprintf(stream,"Size of content:%d Owner:%d Created: %s\n",(int)param->size,param->fd_owner,ctime(&param->creation_time));
}