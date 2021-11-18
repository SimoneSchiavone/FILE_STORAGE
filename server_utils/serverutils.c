// created by Simone Schiavone at 20211007 21:52.
// @Università di Pisa
// Matricola 582418
#include "serverutils.h"

void PrintConfiguration(){
    printf("NWorkers: %d   MaxFilesNum: %d   MaxFilesDim: %d   SocketName: %s   Logfilename: %s\n",n_workers,files_bound,data_bound,socket_name,logfilename);
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
            files_bound=atoi(token);
        }
        if(strncmp(token,"MAX_FILE_DIM",strlen(token))==0){
            token=strtok_r(NULL," \n",&tmpstr);
            //TESTINGGGGGGGGGGg
            data_bound=1100;
            //data_bound=atoi(token)*(1000000);/
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
    files_bound=100;  //numero massimo di files
    data_bound=2048;  //dimensione massima dei files in MBytes
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

int compare_timeval(const struct timeval *timeval_ptr_1, const struct timeval *timeval_ptr_2){
	int cmp;
    //printf("***SX: %ld.%ld DX: %ld.%ld\n",timeval_ptr_1->tv_sec,timeval_ptr_1->tv_usec,timeval_ptr_2->tv_sec,timeval_ptr_2->tv_usec);
	if ((cmp = ((int) (timeval_ptr_1->tv_sec - timeval_ptr_2->tv_sec))) == 0)
		cmp = ((int) (timeval_ptr_1->tv_usec - timeval_ptr_2->tv_usec));
    //printf("***Return %d\n",cmp);
	return(cmp);
}

int FIFO_Replacement(int fd,int send_to_client){
    pthread_mutex_lock(&mutex_storage);

    if(!storage || storage->nentries==0){
        pthread_mutex_unlock(&mutex_storage);
        return -1;
    }

    char* victim_name=NULL; //key della vittima
    char* victim_content=NULL; //contenuto della vittima

    //stored_file* aux;
    struct timeval oldest_time;
    oldest_time.tv_usec=__INT_MAX__;
    oldest_time.tv_sec=__INT_MAX__;
    
    //time_t oldest_time=__INT_MAX__; //tempo di crezione della vittima
    int oldest_size=0; //dimensione della vittima

    entry_t *bucket,*curr_e;
    for(int i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr_e=bucket; curr_e!=NULL;) {
            if(curr_e->key){
                if ((curr_e->data)){
                    //stored_file da analizzare
                    printf("TIMESTAMP %s - %d.%d\n",(char*)(curr_e->key),(int)((((stored_file*)(curr_e->data))->creation_time)).tv_sec,(int)((((stored_file*)(curr_e->data))->creation_time)).tv_usec);
                    if(compare_timeval(&oldest_time,&(((stored_file*)curr_e->data))->creation_time)>0){
                        //Ho trovato il nuovo minimo
                        victim_name=curr_e->key;
                        printf("Nuova vittima %s\n",victim_name);
                        victim_content=(((stored_file*)curr_e->data)->content);
                        oldest_time=(((stored_file*)curr_e->data)->creation_time);
                        oldest_size=((int)((stored_file*)(curr_e->data))->size);
                    }
                    /*
                    if((((stored_file*)(curr_e->data))->creation_time)<oldest_time){
                        //Nuova vittima individuata
                        victim=curr_e->key;
                        oldest_time=(((stored_file*)(curr_e->data))->creation_time);
                        oldest_size=((int)((stored_file*)(curr_e->data))->size);
                    }*/
                }
            }
            curr_e=curr_e->next;
        }
    }
    pthread_mutex_unlock(&mutex_storage);

    printf("--->Devo eliminare il file %s ed il flag di invio e' %d\n",victim_name,send_to_client);

    if(send_to_client){
        //Invio il flag che indica che c'e' un file espulso da inviare al server
        int there_is_a_file_to_send=1,ctrl;
        SYSCALL(ctrl,write(fd,&there_is_a_file_to_send,sizeof(int)),"Errore nella 'write' del flag 0");
        printf("Ho inviato al client il flag 1 per segnalare che c'e' un file da inviargli\n");
        //Invio il nome del file
        int name_size=strlen(victim_name)+1;
        SYSCALL(ctrl,write(fd,&name_size,sizeof(int)),"Errore nell'invio della dimensione del pathname");
        SYSCALL(ctrl,write(fd,victim_name,name_size),"Errore nell'invio del pathname al client");
        printf("Ho inviato al client il nome del file\n");
        //Invio il contenuto del file
        SYSCALL(ctrl,write(fd,&oldest_size,sizeof(int)),"Errore nell'invio della dimensione del pathname");
        SYSCALL(ctrl,write(fd,victim_content,oldest_size),"Errore nell'invio della dimensione del pathname");
        printf("Ho inviato al client il contenuto del file con dimensione %d\n",oldest_size);
    }
    
    pthread_mutex_lock(&mutex_storage);
    int delete=hash_delete(storage,victim_name,NULL,free_stored_file);
    
    if(delete==-1){
        perror("Errore nell'eliminazione del file dallo storage");
        pthread_mutex_unlock(&mutex_storage);
        return -1;
    }else{
        //Aggiorno data_size e nr_entries
        data_size-=oldest_size;
        LOGFILEAPPEND("Vittima designata->%s Dimensione->%d Creata il->%s Correttamente rimossa dallo storage \n",victim_name,oldest_size,ctime(&(oldest_time.tv_sec)));
        LOGFILEAPPEND("Occupazione dello storage dopo l'espulsione %d su %d bytes | Nr file memorizzati %d su %d\n",data_size,data_bound,storage->nentries,files_bound);
        pthread_mutex_unlock(&mutex_storage);
        free(victim_name);
    }
    return 0;
}

int StartReplacementAlgorithm(int fd,int send_to_client){
    //Decidiamo quale politica di rimpiazzamento attuare
    int r=0;
    switch(replacement_policy){
        case 0:
            //Caso FIFO
            printf("Avvio algoritmo di rimpiazzamento FIFO\n");
            LOGFILEAPPEND("Avvio algoritmo di rimpiazzamento FIFO\n");
            if(FIFO_Replacement(fd,send_to_client)==-1){
                return -1;
            }
            break;
        default:
            fprintf(stderr,"Politica di rimpiazzamento sconosciuta");
            return r;
    }
    return r;
}

int StorageUpdateSize(hash_t* ht,int s,int fd,int send_to_client){
    int r=0;
    int need_replacement=0;
    
    pthread_mutex_lock(&mutex_storage);
    need_replacement = (data_size+s<=data_bound) ? 0 : 1;
    pthread_mutex_unlock(&mutex_storage);

    while(need_replacement){
        printf("--NON-- HO SPAZIO A SUFFICIENZA PER MEMORIZZARE\n");
        if(StartReplacementAlgorithm(fd,send_to_client)!=0){
            printf("ALGORITMO DI RIMPIAZZAMENTO FALLITO");
            return -1;
        }
        printf("Rimpiazzamento avvenuto con successo\n");
        r=1; //flag per segnalare che e' intervenuto un algoritmo di rimpiazzamento

        //verifico se e' necessario rimuovere qualche altro file
        need_replacement = (data_size+s<=data_bound) ? 0 : 1;
    }
    
    data_size+=s;
    pthread_mutex_unlock(&mutex_storage);

    //LOGFILEAPPEND("Occupazione dello storage: %d bytes su %d\n",data_size,data_bound);
    
    printf("HO SPAZIO A SUFFICIENZA PER MEMORIZZARE, invio al client uno 0\n");
    int no_file_to_send=0,ctrl;
    SYSCALL(ctrl,write(fd,&no_file_to_send,sizeof(int)),"Errore nella 'write' del flag 0");
    return r;
}

int CanWeStoreAnotherFile(){
    int r;
    pthread_mutex_lock(&mutex_storage);
    if(storage->nentries==files_bound)
        r=0;
    else
        r=1;
    pthread_mutex_unlock(&mutex_storage);
    return r;
}

int IsHoldingTheLock(char* pathname,int fd){
    printf("Sto verificando se %d e' proprietario di %s\n",fd,pathname);
    int r;
    pthread_mutex_lock(&mutex_storage);
    stored_file* found=hash_find(storage,pathname);
    r=(found->fd_holder==fd) ? 1 : 0;
    pthread_mutex_unlock(&mutex_storage);
    return r;
}

int IsUnlocked(char* pathname){
    int r;
    pthread_mutex_lock(&mutex_storage);
    stored_file* found=hash_find(storage,pathname);
    r=(found->fd_holder==-1) ? 1 : 0;
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

        //Creo un nuovo stored_file
        stored_file* new_file=(stored_file*)malloc(sizeof(stored_file));
        if(new_file==NULL){
            perror("Errore nella creazione del nuovo file");
            r.code=-1;
            sprintf(r.message,"Errore nella creazione del nuovo file\n");
        }

        new_file->content=NULL; //Il contenuto è inizialmente nullo

        //Inizializzazione mutex e variabile di condizione
        pthread_mutex_init(&new_file->mutex_file,NULL);
        pthread_cond_init(&new_file->unlocked,NULL);

        //Setting del flag proprietario
        if(o_lock){ //Setto il proprietario
            //TODO: Attenzione al rilascio in modo corretto
            pthread_mutex_lock(&new_file->mutex_file);
            new_file->fd_holder=fd_owner;
        }else{
            new_file->fd_holder=-1;
        }

        new_file->size=0; //Dimensione inizialmente nulla
        gettimeofday(&new_file->creation_time,NULL); //Salvataggio tempo di creazione
        
        //Inserisco nello storage il nuovo stored_file appena creato
        pthread_mutex_lock(&mutex_storage);
        int insert=hash_insert(storage,id,new_file);
        pthread_mutex_unlock(&mutex_storage);
        switch(insert){
            case 1: //null parameters
                perror("[FILE STORAGE] Parametri errati");
                free(new_file);
                r.code=-1;
                sprintf(r.message,"Parametri openFileErrati");
                break;
            case 2: //key già presente
                perror("[FILE STORAGE] Il file è già presente nello storage\n");
                free(new_file);
                r.code=-1;
                sprintf(r.message,"Il file %s è già presente nello storage!",id);
                break;
            case 3: //errore malloc nuovo nodo
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
        pthread_mutex_lock(&mutex_storage);
        stored_file* find=(stored_file*)hash_find(storage,pathname);
        pthread_mutex_unlock(&mutex_storage);
        if(find==NULL){
            perror("[FILE STORAGE] Il file non è presente nello storage\n");
            r.code=-1;
            sprintf(r.message,"Il file %s NON è presente nello storage\n",id);
        }
    }
    return r;
}

response WriteFile(char* pathname,char* content,int size,int fd,int send_to_client){
    response r; 
    if(!pathname){
        r.code=-1;
        sprintf(r.message,"Il path e' nullo\n");
        return r;
    }

    //Verifico se il file esiste nello storage
    pthread_mutex_lock(&mutex_storage);
    stored_file* file=hash_find(storage,pathname);
    
    if(!file){
        //Il file non esiste
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
        //LOGFILEAPPEND("Il file %s non e' stato trovato",pathname);
        //Libero la memoria
        free(pathname);
        free(content);
        return r;
    }else{
        if(file->content!=NULL){
            //Il contenuto del file e' gia' presente
            r.code=-1;
            sprintf(r.message,"Il file %s e' gia' stato scritto",pathname);
            //LOGFILEAPPEND("Il file %s e' gia' stato scritto",pathname);   
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    //Verifico il permesso di scrivere il file
    //posso scrivere il file se questo e' pubblico oppure se sono il proprietario
    /*
    int write_allowed=(file->fd_holder==-1 || fd==file->fd_holder) ? 1 : 0;
    
    if(!write_allowed){ //non ho il permesso di scrivere
        r.code=-1;
        sprintf(r.message,"Non hai i permessi per scrivere il file %s",pathname);
        //Libero la memoria
        free(pathname);
        free(content);
        return r;
    }*/

    //Verifico se lo storage può contenere il file
    if(size>data_bound){
        //Inserisco questo caso in modo che se un file ha una dimensione maggiore di quella
        //massima dello storage non si buttino via tutti i file per far spazio ad uno che
        //e' comunque troppo grande
        r.code=-1;
        sprintf(r.message,"La dimensione del file eccede la dimensione massima dello storage!");
        free(pathname);
        free(content);
        return r;
    }

    if(StorageUpdateSize(storage,size,fd,send_to_client)==-1){
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
    //LOGFILEAPPEND("Sono stati scritti %d bytes nel file con pathname %s\n",size,pathname);
    hash_dump(stdout,storage,print_stored_file_info);
    free(pathname);
    return r;
}

response ReadFile(char* pathname,stored_file** found){
    response r;
    //Verifichiamo se il file e' gia' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    (*found)=hash_find(storage,pathname);
    
    if((*found)==NULL){
        //Il file non e' stato trovato
        fprintf(stderr,"Il file %s non e' stato trovato nello storage\n",pathname);
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);

    }else{ 
        //Il file e' stato trovato, verifichiamo che il contenuto non sia vuoto
        if((*found)->content==NULL){
            //Il file e' vuoto!
            r.code=-1;
            sprintf(r.message,"Il contenuto del file %s e' nullo",pathname);
        }else{
            //Il file non e' vuoto
            r.code=0;
            sprintf(r.message,"Il file %s e' stato trovato nello storage, ecco il contenuto",pathname);
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    return r;
}

response UnlockFile(char* pathname,int fd){
    response r;
    stored_file* found;

    //Verifichiamo se il file e' gia' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    found=(stored_file*)hash_find(storage,pathname);
    pthread_mutex_unlock(&mutex_storage);

    if(!found){ //Il file non e' stato trovato
        //Il file non e' stato trovato
        fprintf(stderr,"Il file %s non e' stato trovato nello storage\n",pathname);
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
    }else{
        //Il file e' stato trovato
        if(found->fd_holder==fd){
            //Sono il proprietario del file
            pthread_mutex_unlock(&found->mutex_file); //rilascio il lock
            found->fd_holder=-1; //annullo il flag
            pthread_cond_signal(&found->unlocked); //notifico i client che aspettano
            r.code=0;
            sprintf(r.message,"Il file %s e' stato correttamente sbloccato",pathname);
        }else{
            //Non sono il proprietario del file
            r.code=-1;
            sprintf(r.message,"Non puoi sbloccare il file %s",pathname);
        }
    }
    return r;
}

response LockFile(char* pathname,int fd){
    response r;
    stored_file* found;

    //Verifichiamo se il file e' gia' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    found=(stored_file*)hash_find(storage,pathname);
    pthread_mutex_unlock(&mutex_storage);

    if(found==NULL){ //Il file non e' stato trovato
        //Il file non e' stato trovato
        fprintf(stderr,"Il file %s non e' stato trovato nello storage\n",pathname);
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
    }else{
        //Il file e' stato trovato
        pthread_mutex_lock(&found->mutex_file);
        found->fd_holder=fd;
        /*
        while(found->fd_holder!=-1){
            pthread_cond_wait(&found->unlocked,NULL); //aspetto che sia libero
        }*/
    }
    r.code=0;
    sprintf(r.message,"Il file %s e' ora bloccato dal fd %d",pathname,fd);
    return r;
}

int UnlockAllMyFiles(int fd){
    entry_t *bucket, *curr;
    int i;
    pthread_mutex_lock(&mutex_storage);
    if(!storage)
        return -1;
    for(i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key){
                stored_file* sf=(stored_file*)curr->data;
                printf("File %s Proprietario %d\n",(char*)curr->key,sf->fd_holder);
                if(sf->fd_holder==fd){
                    //Sono il proprietario del file
                    pthread_mutex_unlock(&sf->mutex_file);
                    sf->fd_holder=-1; //annullo il flag
                    pthread_cond_signal(&sf->unlocked); //notifico i client che aspettano
                    printf("Ho sbloccato il file %s che era stato bloccato dal fd %d\n",(char*)curr->key,fd);
                }
            }
            curr=curr->next;
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    return 0;
}

response RemoveFile(char* pathname,int fd){
    response r;
    stored_file* found; 
    //Verifichiamo se il file e' gia' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    found=(stored_file*)hash_find(storage,pathname);
    pthread_mutex_unlock(&mutex_storage);

    if(!found){
        r.code=-1;
        sprintf(r.message,"Il file %s non e' presente nello storage",pathname);
        return r;
    }
    int dim=found->size; //Memorizzo la dimensione del contenuto

    //Verifichiamo se abbiamo i diritti di accesso
    if(!IsHoldingTheLock(pathname,fd)){
        if(IsUnlocked(pathname)){
            r.code=-1;
            sprintf(r.message,"Non puoi cancellare un file pubblico");
        }else{
            r.code=-1;
            sprintf(r.message,"Non puoi cancellare un file bloccato da un altro client");
        }
        return r;
    }

    //Procedo all'eliminazione del file
    pthread_mutex_lock(&mutex_storage);
    if(hash_delete(storage,pathname,free,free_stored_file)==-1){
        r.code=-1;
        sprintf(r.message,"Errore nella procedura di eliminazione del file dallo storage");
    }
    data_size-=dim;
    pthread_mutex_unlock(&mutex_storage);

    r.code=0;
    sprintf(r.message,"Il file %s e' stato correttamente rimosso",pathname);
    return r;
}

int ExecuteRequest(int fun,int fd){
    switch(fun){
        case (3):{ //Operazione Open File
            printf("\n\n*****Operazione OPENFILE Fd: %d*****\n",fd);
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
        case (4):{ //Operazione Read File
            printf("\n\n*****Operazione READFILE Fd: %d*****\n",fd);
            int intbuffer; //Buffer di interi
            int ctrl; //variabile che memorizza il risultato di ritorno di chiamate di sistema/funzioni

            //Leggo la dimensione del pathname e poi leggo il pathname
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST-ReadFile] Errore nella 'read' della dimensione del pathname");
            char* stringbuffer; //buffer per contenere la stringa
            if(!(stringbuffer=(char*)calloc(intbuffer,sizeof(char)))){ //Verifica malloc
                perror("[EXECUTE REQUEST-ReadFile] Errore nella 'malloc' del buffer per pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,stringbuffer,intbuffer),"[EXECUTE REQUEST-ReadFile] Errore nella 'read' del pathname");
            printf("[EXECUTE REQUEST-ReadFile] Ho ricevuto il path %s di dimensione %d\n",stringbuffer,ctrl);
            
            stored_file* found=NULL;
            response r=ReadFile(stringbuffer,&found);
            free(stringbuffer);
            printf("Codice %d Messaggio %s Locazione dello file %p\n",r.code,r.message,(void*)found);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST-ReadFile] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST-ReadFile] Errore nella 'write' della risposta");

            if(r.code==0){
                responsedim=(int)found->size;
                printf("Dimensione del contenuto %d\n",responsedim);
                //Invio prima la dimensione della risposta e poi la risposta
                SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST-ReadFile] Errore nella 'write' della dimensione del file da inviare");
                printf("Ho scritto %d bytes cioe' %d\n",ctrl,responsedim);
                SYSCALL(ctrl,write(fd,found->content,responsedim),"[EXECUTE REQUEST-ReadFile] Errore nella 'write' del contenuto del file");
                printf("Ho scritto %d bytes cioe' %s\n",ctrl,found->content);
            }
            return r.code;
        }
        case (6):{ //Operazione Write File
            printf("\n\n*****Operazione WRITEFILE Fd: %d*****\n",fd);
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
            
            //-----Lettura del flag di invio degli eventuali file espulsi
            int flag;
            SYSCALL(ctrl,read(fd,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
            printf("Ho ricevuto il flag %d\n",flag);

            //Verifica autorizzazione ed invio esito
            int authorized=IsHoldingTheLock(read_name,fd);
            SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
            printf("HO INVIATO IL BIT DI AUTORIZZAZIONE %d di dimensione %d\n",authorized,ctrl);
            if(!authorized){
                free(read_file);
                free(read_name);
                return -1;
            }

            //-----Esecuzione operazione
            response r=WriteFile(read_name,read_file,intbuffer,fd,flag);

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
        case (8):{ //Operazione Lock File
            printf("\n\n*****Operazione LOCK FILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 8] Errore nella 'read' della dimensione del pathname");
            char* string_buffer=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(string_buffer==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,string_buffer,intbuffer),"[EXECUTE REQUEST Case 8] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",string_buffer);

            response r=LockFile(string_buffer,fd);
            free(string_buffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST Case 8] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST Case 8] Errore nella 'write' della risposta");

            return r.code;
        } 
        case (9):{ //Operazione Unlock File
            printf("\n\n*****Operazione UNLOCK FILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 8] Errore nella 'read' della dimensione del pathname");
            char* string_buffer=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(string_buffer==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,string_buffer,intbuffer),"[EXECUTE REQUEST Case 8] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",string_buffer);

            response r=UnlockFile(string_buffer,fd);
            free(string_buffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST Case 8] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST Case 8] Errore nella 'write' della risposta");
            return r.code;
        }
        case(11):{ //Operazione Remove File
            printf("\n\n*****Operazione REMOVE FILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 11] Errore nella 'read' della dimensione del pathname");
            char* string_buffer=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(string_buffer==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,string_buffer,intbuffer),"[EXECUTE REQUEST Case 11] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",string_buffer);

            response r=RemoveFile(string_buffer,fd);
            free(string_buffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST Case 11] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST Case 11 Errore nella 'write' della risposta");
            return r.code;
        }
        case (12):{ //Operazione "Ho finito"
            printf("\n\n*****Operazione CHIUSURA Fd: %d*****\n",fd);  
            if(UnlockAllMyFiles(fd)!=0)
                return -1;
            return 1;
        }
        default:{ //Operazione Sconosciuta
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
    fprintf(stream,"Size of content:%d Owner:%d Created: %s\n",(int)param->size,param->fd_holder,ctime(&(param->creation_time).tv_sec));
}