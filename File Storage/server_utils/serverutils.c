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
        return -1;
    }

    char line[128];
    memset(line,'\0',128);
    //Leggiamo il file
    while(fgets(line,128,configfile)!=NULL){
        
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
            data_bound=((int)strtol(token,NULL,10))*(1000000);
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
            }else{
                if(strncmp(token,"lru",3)==0){
                    replacement_policy=1;
                }
                else{
                    //Errato valore per la politica di rimpiazzamento
                    fprintf(stderr,"!!! Politica di rimpiazzamento sconosciuta - Partenza con parametri di default\n");
                    DefaultConfiguration();
                    break;
                }
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
    strncpy(socket_name,"./SocketFileStorage.sock",25); //nome del socket AF_UNIX
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
    if(pthread_cond_init(&list_not_empty,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return;
    }
    max_connections_bound=5;
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

int InitializeStorage(){
    if((storage= hash_create(15,hash_pjw,string_compare))==NULL)
        return -1;
    if(pthread_mutex_init(&mutex_storage,NULL)!=0){
        perror("Errore nell'inizializzazione del mutex");
        return -1;
    }
    nr_of_replacements=0;
    max_data_size=-1;
    max_data_num=-1;
    not_empty_files_num=0;
    return 0;
}

void free_stored_file(void* tf){
    stored_file* to_free=(stored_file*)tf;
    if(to_free!=NULL){
        if(to_free->content){
            free(to_free->content);
            not_empty_files_num--;
        }
        list_destroy(to_free->opened_by);
        free(to_free);
    }
}

int DestroyStorage(){
    return hash_destroy(storage,free,free_stored_file);
}

int compare_timeval(const struct timeval *timeval_ptr_1, const struct timeval *timeval_ptr_2){
	int cmp;
	if ((cmp = ((int) (timeval_ptr_1->tv_sec - timeval_ptr_2->tv_sec))) == 0)
		cmp = ((int) (timeval_ptr_1->tv_usec - timeval_ptr_2->tv_usec));
	return(cmp);
}

int FIFO_Replacement(int fd,int send_to_client,char* do_not_remove){
    if(!storage || storage->nentries==0){
        return -1;
    }

    int check= (do_not_remove) ? 1 : 0;

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
                    //printf("TIMESTAMP %s - %d.%d\n",(char*)(curr_e->key),(int)((((stored_file*)(curr_e->data))->creation_time)).tv_sec,(int)((((stored_file*)(curr_e->data))->creation_time)).tv_usec);
                    if(compare_timeval(&oldest_time,&(((stored_file*)curr_e->data))->creation_time)>0){
                        //Ho trovato il nuovo minimo
                        if(check && (strncmp(curr_e->key,do_not_remove,strlen(curr_e->key))==0)){
                            //Ignoro questo file perche' e' di mio interesse e non voglio eliminarlo
                        }else{
                            victim_name=curr_e->key;
                            printf("Nuova vittima %s\n",victim_name);
                            victim_content=(((stored_file*)curr_e->data)->content);
                            oldest_time=(((stored_file*)curr_e->data)->creation_time);
                            oldest_size=((int)((stored_file*)(curr_e->data))->size);
                        }
                    }
                }
            }
            curr_e=curr_e->next;
        }
    }

    printf("--->Devo eliminare il file %s ed il flag di invio e' %d\n",victim_name,send_to_client);

    if(send_to_client){
        //Invio il flag che indica che c'e' un file espulso da inviare al server
        int there_is_a_file_to_send=1,ctrl;
        SYSCALL(ctrl,write(fd,&there_is_a_file_to_send,sizeof(int)),"Errore nella 'write' del flag 0");
        //printf("Debug, ho inviato al client il flag 1 per segnalare che c'e' un file da inviargli\n");

        //Invio il nome del file
        int name_size=strlen(victim_name)+1;
        SYSCALL(ctrl,write(fd,&name_size,sizeof(int)),"Errore nell'invio della dimensione del pathname");
        //printf("Debug, ho inviato al client %d bytes di dimensione del nome del file espulso %d\n",ctrl,name_size);
        SYSCALL(ctrl,write(fd,victim_name,name_size),"Errore nell'invio del pathname al client");
        //printf("Debug, ho inviato al client %d bytes di nome del file espulso %s\n",ctrl,victim_name);

        //Invio il contenuto del file
        SYSCALL(ctrl,write(fd,&oldest_size,sizeof(int)),"Errore nell'invio della dimensione del pathname");
        //printf("Debug, ho inviato al client %d bytes di dimensione del contenuto del file espulso %d\n",ctrl,oldest_size);
        SYSCALL(ctrl,write(fd,victim_content,oldest_size),"Errore nell'invio della dimensione del pathname");
        //printf("Debug, ho inviato al client %d bytes di contenuto del file espulso %s\n",ctrl,victim_content);
    }
    
    int delete=hash_delete(storage,victim_name,NULL,free_stored_file);
    
    if(delete==-1){
        perror("Errore nell'eliminazione del file dallo storage");
        return -1;
    }else{
        //Aggiorno data_size e nr_entries
        data_size-=oldest_size;
        LOGFILEAPPEND("Politica di rimpiazzamento FIFO\nVittima designata->%s Dimensione->%d Creata il->%s Correttamente rimossa dallo storage \nOccupazione dello storage dopo l'espulsione %d su %d bytes | Nr file memorizzati %d su %d\n",victim_name,oldest_size,ctime(&(oldest_time.tv_sec)),data_size,data_bound,storage->nentries,files_bound);
        free(victim_name);
    }
    return 0;
}

int LRU_Replacement(int fd,int send_to_client,char* do_not_remove){
    if(!storage || storage->nentries==0){
        return -1;
    }

    int check= (do_not_remove) ? 1 : 0;

    char* victim_name=NULL; //key della vittima
    char* victim_content=NULL; //contenuto della vittima

    //stored_file* aux;
    struct timeval oldest_time;
    oldest_time.tv_usec=__INT_MAX__;
    oldest_time.tv_sec=__INT_MAX__;
    
    int oldest_size=0; //dimensione della vittima

    entry_t *bucket,*curr_e;
    for(int i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr_e=bucket; curr_e!=NULL;) {
            if(curr_e->key){
                if ((curr_e->data)){
                    //stored_file da analizzare
                    //printf("TIMESTAMP %s - %d.%d\n",(char*)(curr_e->key),(int)((((stored_file*)(curr_e->data))->last_operation)).tv_sec,(int)((((stored_file*)(curr_e->data))->last_operation)).tv_usec);
                    if(compare_timeval(&oldest_time,&(((stored_file*)curr_e->data))->last_operation)>0){
                        //Ho trovato il nuovo minimo
                        if(check && (strncmp(curr_e->key,do_not_remove,strlen(curr_e->key))==0)){
                            //Ignoro questo file perche' e' di mio interesse e non voglio eliminarlo
                        }else{
                            victim_name=curr_e->key;
                            //printf("Nuova vittima %s\n",victim_name);
                            victim_content=(((stored_file*)curr_e->data)->content);
                            oldest_time=(((stored_file*)curr_e->data)->last_operation);
                            oldest_size=((int)((stored_file*)(curr_e->data))->size);
                        }
                    }
                }
            }
            curr_e=curr_e->next;
        }
    }

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
        printf("Ho inviato al client %d bytes di dimensione del contenuto del file %d\n",ctrl,oldest_size);
        SYSCALL(ctrl,write(fd,victim_content,oldest_size),"Errore nell'invio della dimensione del pathname");
        printf("Ho inviato al client %d bytes di il contenuto del file\n",ctrl);
    }
    
    int delete=hash_delete(storage,victim_name,NULL,free_stored_file);
    
    if(delete==-1){
        perror("Errore nell'eliminazione del file dallo storage");
        return -1;
    }else{
        //Aggiorno data_size e nr_entries
        data_size-=oldest_size;
        LOGFILEAPPEND("Politica di rimpiazzamento LRU\nVittima designata->%s Dimensione->%d Creata il->%s Correttamente rimossa dallo storage \nOccupazione dello storage dopo l'espulsione %d su %d bytes | Nr file memorizzati %d su %d\n",victim_name,oldest_size,ctime(&(oldest_time.tv_sec)),data_size,data_bound,storage->nentries,files_bound);
        free(victim_name);
    }
    return 0;
}

int StartReplacementAlgorithm(int fd,int send_to_client,char* do_not_remove){
    nr_of_replacements++;
    //Decidiamo quale politica di rimpiazzamento attuare
    int r=0;
    switch(replacement_policy){
        case 0:{ //Caso FIFO
            printf("Avvio algoritmo di rimpiazzamento FIFO\n");
            LOGFILEAPPEND("Avvio algoritmo di rimpiazzamento FIFO\n");
            if(FIFO_Replacement(fd,send_to_client,do_not_remove)==-1){
                return -1;
            }
            break;
        }
        case 1:{ //Caso LRU
            printf("Avvio algoritmo di rimpiazzamento LRU\n");
            LOGFILEAPPEND("Avvio algoritmo di rimpiazzamento LRU\n");
            if(LRU_Replacement(fd,send_to_client,do_not_remove)==-1){
                return -1;
            }
            break;
        }
        default:
            fprintf(stderr,"Politica di rimpiazzamento sconosciuta");
            return r;
    }
    return r;
}

int StorageUpdateSize(hash_t* ht,int s,int fd,int send_to_client,char* do_not_remove){
    int r=0;
    int need_replacement=0;
    
    need_replacement = (data_size+s<=data_bound) ? 0 : 1;
    printf("DEBUG\n DataSize %d DataSize+s %d DataBound %d\n",data_size,data_size+s,data_bound);
    while(need_replacement){
        printf("--NON-- HO SPAZIO A SUFFICIENZA PER MEMORIZZARE\n");
        if(StartReplacementAlgorithm(fd,send_to_client,do_not_remove)!=0){
            printf("ALGORITMO DI RIMPIAZZAMENTO FALLITO");
            return -1;
        }
        printf("Rimpiazzamento avvenuto con successo\n");
        r=1; //flag per segnalare che e' intervenuto un algoritmo di rimpiazzamento

        //verifico se e' necessario rimuovere qualche altro file
        need_replacement = (data_size+s<=data_bound) ? 0 : 1;
    }
    
    data_size+=s;
    if(data_size>max_data_size)
        max_data_size=data_size;
        
    LOGFILEAPPEND("Occupazione dello storage: %d bytes su %d\n",data_size,data_bound);
    
    if(send_to_client){
        printf("HO SPAZIO A SUFFICIENZA PER MEMORIZZARE, invio al client uno 0\n");
        int no_file_to_send=0,ctrl;
        SYSCALL(ctrl,write(fd,&no_file_to_send,sizeof(int)),"Errore nella 'write' del flag 0");
    }
    return r;
}

int CanWeStoreAnotherFile(){
    int r;
    printf("HT Entryes %d , FilesBound %d\n",storage->nentries,files_bound);
    if(storage->nentries==files_bound)
        r=0;
    else
        r=1;
    return r;
}

response LockFile(char* pathname,int fd,int from_openFile){
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
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage, non lo si puo' bloccare\n",pathname);
        return r;
    }else{
        //Il file e' stato trovato
        pthread_mutex_lock(&found->mutex_file);
    
        //Verifico che il client abbia aperto il file
        //Se provengo dalla OpenFile tale controllo viene saltato
        if((!from_openFile) && !(is_in_list(found->opened_by,fd))){
            r.code=-1;
            sprintf(r.message,"Non hai aperto il file %s",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s non puo' essere bloccato perche' non e' stato aperto\n",fd,pathname);
            pthread_mutex_unlock(&found->mutex_file);
            return r;
        }

        //Verifico se il client ha gia' bloccato quel file
        if(found->fd_holder==fd){
            r.code=0;
            sprintf(r.message,"OK, Hai gia' bloccato il file %s\n",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s non puo' essere bloccato di nuovo perche' l'ha gia' bloccato\n",fd,pathname);
            pthread_mutex_unlock(&found->mutex_file);
            return r;
        }
        found->clients_waiting++;
        while(found->fd_holder!=-1 && found->to_delete==0){ 
            //Finche' il file e' bloccato e non deve essere eliminato
            printf("Client %d aspetta che il file si liberi!\n",fd);
            pthread_cond_wait(&found->is_unlocked,&found->mutex_file); //aspetto che sia libero
        }
        found->clients_waiting--;

        if(!found->to_delete){
            found->fd_holder=fd;
            r.code=0;
            sprintf(r.message,"OK, Il file %s e' ora bloccato dal fd %d",pathname,fd);
            LOGFILEAPPEND("[Client %d] Il file %s e' stato correttamente bloccato\n",fd,pathname);
            gettimeofday(&found->last_operation,NULL); 
            pthread_mutex_unlock(&found->mutex_file);
            return r;
        }else{
            //Se il file e' da eliminare non puo' essere bloccato da altri client
            r.code=-1;
            sprintf(r.message,"Il file %s deve essere eliminato, non lo si puo' bloccare",pathname);
            printf("Il file %s deve essere eliminato, non posso piu' bloccarlo\n",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s deve essere eliminato, non posso bloccarlo\n",fd,pathname);
            //Avviso il proprietario che non sono piu' interessato a bloccare il file quindi puo' eliminarlo
            printf("Avviso il proprietario che non voglio piu' bloccare il file %s\n",pathname);
            pthread_cond_signal(&found->is_deletable);
            pthread_mutex_unlock(&found->mutex_file);
            return r;
        }
    }
    
}

/*Richiesta di apertura o creazione di un file.*/
response OpenFile(char* pathname, int o_create,int o_lock,int fd_owner){
    response r;
    r.code=0;

    printf("[FILE STORAGE-OpenFile] Il path ricevuto e' %s\n",pathname);
    if(!pathname){
        r.code=-1;
        sprintf(r.message,"Il path ricevuto e' nullo");
        LOGFILEAPPEND("[Client %d] Errore nella OpenFile richiesta\n",fd_owner);
        return r;
    }
    
    if(o_create){ //Il file va creato
        //Acquisisco il mutex sullo storage
        pthread_mutex_lock(&mutex_storage);

        stored_file* found=hash_find(storage,pathname);
        if(found!=NULL){
            fprintf(stderr,"%s e' gia' presente nello storage\n",pathname);
            r.code=-1;
            sprintf(r.message,"Il file %s è gia' presente nello storage!",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s e' gia' presente nello storage!\n",fd_owner,pathname);
            pthread_mutex_unlock(&mutex_storage);
            return r;
        }

        //Verifichiamo che sia possibile creare un nuovo file nello storage (condizione sul nr max di file)
        if(!CanWeStoreAnotherFile()){
            printf("Non e' possibile memorizzare ulteriori file nello storage (max numero file archiviati raggiunto), procedo all'eliminazione di un file\n");
            if(StartReplacementAlgorithm(fd_owner,0,NULL)==-1){
                r.code=-1;
                sprintf(r.message,"Non e' possibile memorizzare ulteriori file nello storage (max numero file archiviati raggiunto)\n");
                LOGFILEAPPEND("[Client %d] Non e' possibile memorizzare il file %s per raggiunta capacità massima dello storage (max numero file archiviati raggiunto)\n",fd_owner,pathname);
                pthread_mutex_unlock(&mutex_storage);
                return r;
            }
        }

        //Creo un nuovo stored_file
        char* id=strndup(pathname,strlen(pathname));
        stored_file* new_file=(stored_file*)malloc(sizeof(stored_file));
        if(new_file==NULL){
            perror("Errore nella creazione del nuovo file");
            r.code=-1;
            sprintf(r.message,"Errore nella creazione del nuovo file\n");
            LOGFILEAPPEND("[Client %d] Errore nella OpenFile\n",fd_owner);
            return r;
        }

        new_file->content=NULL; //Il contenuto è inizialmente nullo
        new_file->clients_waiting=0; //Nessun client sta aspettando di acquisire questa lock
        new_file->to_delete=0; //Non e' in fase di eliminazione
        new_file->opened_by=NULL;
        if(list_push(&new_file->opened_by,fd_owner)==0){
            //printf("Inserimento corretto in lista\n");
        }else{
            printf("ERRORE IN INSERIMENTO IN LISTA");
        }

        //Inizializzazione mutex e variabile di condizione
        pthread_mutex_init(&new_file->mutex_file,NULL);
        pthread_cond_init(&new_file->is_unlocked,NULL);
        pthread_cond_init(&new_file->is_deletable,NULL);

        //Setting del flag proprietario
        if(o_lock){ 
            new_file->fd_holder=fd_owner; //Setto il proprietario
        }else{
            new_file->fd_holder=-1; //Non locked
        }

        new_file->size=0; //Dimensione inizialmente nulla
        gettimeofday(&new_file->creation_time,NULL); //Salvataggio tempo di creazione
        gettimeofday(&new_file->last_operation,NULL); //Salvataggio tempo di ultima operazione
        
        //Inserisco nello storage il nuovo stored_file appena creato
        int insert=hash_insert(storage,id,new_file);

        switch(insert){
            case 1:{ //null parameters
                fprintf(stderr,"Parametri errati");
                if(new_file){
                    list_destroy(new_file->opened_by);
                    free(new_file);
                }
                if(id)
                    free(id);
                r.code=-1;
                sprintf(r.message,"Parametri openFileErrati");
                LOGFILEAPPEND("[Client %d] Errore nella OpenFile\n",fd_owner);
                break;
            }
            case 2:{ //key già presente
                fprintf(stderr,"%s e' gia' presente nello storage\n",pathname);
                r.code=-1;
                sprintf(r.message,"Il file %s è gia' presente nello storage!",id);
                LOGFILEAPPEND("[Client %d] Il file %s e' gia' presente nello storage!\n",fd_owner,pathname);
                list_destroy(new_file->opened_by);
                free(new_file);
                free(id);

                break;
            }
            case 3:{ //errore malloc nuovo nodo
                fprintf(stderr,"Malloc nuova entry hash table");
                r.code=-1;
                sprintf(r.message,"Errore nella malloc del nuovo file");
                LOGFILEAPPEND("[Client %d] Errore nella OpenFile\n",fd_owner);
                list_destroy(new_file->opened_by);
                free(new_file);
                free(id);
                break;
            }
            case 0:{
                printf("[FILE STORAGE] %s inserito nello storage\n",id);

                //Aggiornamento max_data_num
                if((storage->nentries)>max_data_num){
                    max_data_num=(storage->nentries);
                }

                hash_dump(stdout,storage,print_stored_file_info);

                r.code=0;
                sprintf(r.message,"OK, Il file %s e' stato inserito nello storage",id);
                if(o_lock){
                    LOGFILEAPPEND("[Client %d] Il file %s e' stato creato, inserito nello storage e bloccato\n",fd_owner,pathname);
                }else{
                    LOGFILEAPPEND("[Client %d] Il file %s e' stato creato ed inserito nello storage dal client %d\n",fd_owner,pathname);
                }
            }
        }
        pthread_mutex_unlock(&mutex_storage);
    }else{ //Il file deve essere presente
        /*
        stored_file* find=(stored_file*)hash_find(storage,pathname);
        pthread_mutex_unlock(&mutex_storage);
        if(find==NULL){
            fprintf(stderr,"Il file %s non e' presente nello storage\n",pathname);
            r.code=-1;
            sprintf(r.message,"Il file %s non e' presente nello storage\n",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s non e' presente nello storage\n",fd_owner,pathname);
        }else{
            if(o_lock){
                pthread_mutex_lock(&find->mutex_file);
                new_file->fd_holder=fd_owner; //Setto il proprietario
             }else{
                new_file->fd_holder=-1; //Non locked
            }
        }*/

        if(o_lock){
            pthread_mutex_lock(&mutex_storage);
            stored_file* file=hash_find(storage,pathname);
            pthread_mutex_unlock(&mutex_storage);
            if(file){ //File trovato
                response lock=LockFile(pathname,fd_owner,1);
                if(lock.code!=0){
                    r.code=-1;
                    strcpy(r.message,lock.message);
                    return r;
                }else{
                    pthread_mutex_lock(&file->mutex_file);
                    if(list_push(&file->opened_by,fd_owner)==0){
                        printf("Inserimento corretto in lista\n");
                    }else{
                        printf("ERRORE IN INSERIMENTO IN LISTA");
                    }
                    pthread_mutex_unlock(&file->mutex_file);
                    r.code=0;
                    sprintf(r.message,"OK, il file %s e' stato bloccato",pathname);
                    return r;
                }
            }
        }else{
            pthread_mutex_lock(&mutex_storage);
            stored_file* file=hash_find(storage,pathname);
            pthread_mutex_unlock(&mutex_storage);

            if(file){ //File trovato
                pthread_mutex_lock(&file->mutex_file);
                if(list_push(&file->opened_by,fd_owner)==0){
                    printf("Inserimento corretto in lista\n");
                }else{
                    printf("ERRORE IN INSERIMENTO IN LISTA");
                }
                pthread_mutex_unlock(&file->mutex_file);
                r.code=0;
                sprintf(r.message,"OK, il file %s e' stato aperto",pathname);
                return r;
            }else{ //File non trovato
                r.code=-1;
                sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
                return r;
            }
        }
        
    }
    return r;
}

response WriteFile(char* pathname,char* content,int size,int fd,int send_to_client){
    response r; 
    //Controllo parametro
    if(!pathname || !content){
        r.code=-1;
        sprintf(r.message,"Parametro nullo!");
        LOGFILEAPPEND("[Client %d] Errore nella WriteFile\n",fd);
        int no_file=0,ctrl;
        SYSCALL(ctrl,write(fd,&no_file,sizeof(int)),"Errore nella scrittura della risposta");
        return r;
    }

    //Verifico se il file esiste nello storage
    pthread_mutex_lock(&mutex_storage);
    stored_file* file=hash_find(storage,pathname);
    

    if(!file){ //Il file non esiste
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s non e' presente nello storage, non e' possibile scrivere il contenuto\n",fd,pathname);
        free(pathname);
        free(content);
        pthread_mutex_unlock(&mutex_storage);
        int no_file=0,ctrl;
        SYSCALL(ctrl,write(fd,&no_file,sizeof(int)),"Errore nella scrittura della risposta");
        return r;
    }else{ //Il file esiste nello storage
        pthread_mutex_lock(&file->mutex_file);

        //Verifichiamo l'autorizzazione all'accesso
        int authorized=(file->fd_holder==fd || !is_in_list(file->opened_by,fd)) ? 1 : 0;
        if(!authorized){
            pthread_mutex_unlock(&mutex_storage);
            pthread_mutex_unlock(&file->mutex_file);
            r.code=-1;
            sprintf(r.message,"Non hai i permessi per scrivere il file %s",pathname);
            //Libero la memoria
            LOGFILEAPPEND("[Client %d] Non si dispone dell'autorizzazione per scrivere il file %s\n",fd,pathname);
            int ctrl;
            SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
            free(pathname);
            free(content);
            return r; 
        }else{
            //Verifichiamo se esista gia' un contenuto
            if(file->content!=NULL){
                pthread_mutex_unlock(&mutex_storage);
                pthread_mutex_unlock(&file->mutex_file);
                //Il contenuto del file e' gia' presente
                r.code=-1;
                sprintf(r.message,"Il file %s ha gia' del contenuto",pathname);
                //Libero la memoria
                LOGFILEAPPEND("[Client %d] Il file %s ha gia' un contenuto e non puo' essere scritto\n",fd,pathname);
                free(pathname);
                free(content);
                int ctrl;
                authorized=0;
                SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
                return r;
            }else{
                //Verifico se lo storage puo' contenere il file
                if(size>data_bound){
                    //Inserisco questo caso in modo che se un file ha una dimensione maggiore di quella
                    //massima dello storage non si buttino via tutti i file per far spazio ad uno che
                    //e' comunque troppo grande
                    pthread_mutex_unlock(&mutex_storage);
                    pthread_mutex_unlock(&file->mutex_file);
                    r.code=-1;
                    sprintf(r.message,"La dimensione del file eccede la dimensione massima dello storage!");
                    LOGFILEAPPEND("[Client %d] Non vi e' spazio a sufficienza nello storage per memorizzare il contenuto del file %s\n",fd,pathname);
                    free(pathname);
                    free(content);
                    int ctrl;
                    authorized=0;
                    SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
                    return r;
                }
            }
        }

        //se i controlli precedenti sono andati a buon fine mandiamo l'autorizzazione a procedere
        int ctrl;
        SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
    }

    //Faccio spazio nello storage
    if(StorageUpdateSize(storage,size,fd,send_to_client,pathname)==-1){
        r.code=-1;
        sprintf(r.message,"Errore nell'attuazione della politica di rimpiazzamento");
        free(pathname);
        free(content);
        pthread_mutex_unlock(&mutex_storage);
        pthread_mutex_unlock(&file->mutex_file);
        return r;
    }

    //Imposto contenuto e dimensione del file
    file->content=content;
    file->size=(size_t)size;
    
    /*
    printf("Crea:%s Last:%s\n",ctime(&(file->creation_time).tv_sec),ctime(&(file->last_operation).tv_sec));
    sleep(2);*/
    gettimeofday(&file->last_operation,NULL); //Salvataggio tempo di ultima operazione
    //printf("Creat:%d.%ld Last:%d.%ld\n",(int)file->creation_time.tv_sec,file->creation_time.tv_usec,(int)file->last_operation.tv_sec,file->last_operation.tv_usec);
    
    pthread_mutex_unlock(&file->mutex_file);

    not_empty_files_num++;
    r.code=0;
    sprintf(r.message,"OK, Il contenuto del file %s e' stato scritto sullo storage",pathname);
    LOGFILEAPPEND("[Client %d] Sono stati scritti %d bytes nel file con pathname %s\n",fd,size,pathname);
    hash_dump(stdout,storage,print_stored_file_info);
    //Ho terminato le operazioni sullo storage, rilascio il lock
    pthread_mutex_unlock(&mutex_storage);
    free(pathname);
    return r;
}

response AppendToFile(char* pathname,char* content_to_append,int size,int fd,int send_to_client){
    response r; 
    //Controllo parametro
    if(!pathname || !content_to_append){
        r.code=-1;
        sprintf(r.message,"Parametro nullo!");
        printf("PARAMETRO NULLO!\n");
        LOGFILEAPPEND("[Client %d] Errore nella AppendToFile\n",fd);
        int no_file=0,ctrl;
        //no autorizzazione
        SYSCALL(ctrl,write(fd,&no_file,sizeof(int)),"Errore nella scrittura della risposta");
        return r;
    }

    //Verifico se il file esiste nello storage
    pthread_mutex_lock(&mutex_storage);
    stored_file* file=hash_find(storage,pathname);
    
    int newdim;

    if(!file){ //Il file non esiste
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
        printf("Il file %s non e' stato trovato nello storage!\n",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage\n",fd,pathname);
        //Libero la memoria
        free(pathname);
        free(content_to_append);
        pthread_mutex_unlock(&mutex_storage);
        int no_file=0,ctrl;
        SYSCALL(ctrl,write(fd,&no_file,sizeof(int)),"Errore nella scrittura della risposta");
        return r;

    }else{ //Il file esiste nello storage
        pthread_mutex_lock(&file->mutex_file);

        //Verifichiamo l'autorizzazione (apertura del file e lock)
        int authorized=(file->fd_holder==fd && is_in_list(file->opened_by,fd)) ? 1 : 0;
        
        if(!authorized){
            pthread_mutex_unlock(&mutex_storage);
            pthread_mutex_unlock(&file->mutex_file);
            r.code=-1;
            sprintf(r.message,"Non hai i permessi per scrivere il file %s",pathname);
            printf("No autorizzazione\n");
            LOGFILEAPPEND("[Client %d] Non si dispone dell'autorizzazione per aggiungere contenuto al file %s\n",fd,pathname);
            //Libero la memoria
            free(pathname);
            free(content_to_append);
            int ctrl;
            SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
            return r; 
        }else{
            //Verifichiamo se esiste gia' un contenuto
            if(file->content==NULL){
                pthread_mutex_unlock(&mutex_storage);
                pthread_mutex_unlock(&file->mutex_file);
                //Il contenuto e' nullo
                r.code=-1;
                sprintf(r.message,"Il file %s non ha un contenuto",pathname);
                printf("Il file %s non ha un contenuto\n",pathname);
                LOGFILEAPPEND("[Client %d] Il file %s non ha contenuto, non e' possibile appendere nuovo contenuto\n",fd,pathname);
                //Libero la memoria
                free(pathname);
                free(content_to_append);
                authorized=0;
                int ctrl;
                SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella scrittura della risposta");
                return r; 
            }else{  
                newdim=((int)(file->size))+size-1;
                //Verifico se lo storage puo' contenere il file
                if(newdim>data_bound){
                    //Inserisco questo caso in modo che se un file ha una dimensione maggiore di quella
                    //massima dello storage non si buttino via tutti i file per far spazio ad uno che
                    //e' comunque troppo grande
                    pthread_mutex_unlock(&mutex_storage);
                    pthread_mutex_unlock(&file->mutex_file);
                    r.code=-1;
                    sprintf(r.message,"La dimensione del file eccede la dimensione massima dello storage!");
                    printf("La dimensione dell'append supera la dimensione massima dello storage\n");
                    LOGFILEAPPEND("[Client %d] Non vi e' spazio a sufficienza nello storage per memorizzare il contenuto del file %s\n",fd,pathname);
                    free(pathname);
                    free(content_to_append);
                    int ctrl;
                    authorized=0;
                    SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella scrittura della risposta");
                    return r;
                }
            }

            //se i controlli precedenti sono andati a buon fine mandiamo l'autorizzazione a procedere
            int ctrl;
            SYSCALL(ctrl,write(fd,&authorized,sizeof(int)),"Errore nella 'write' dell'autorizzazione");
        }

        
    }

    

    //Faccio spazio nello storage solo per quanti bytes devo appendere cioe' size
    if(StorageUpdateSize(storage,size,fd,send_to_client,pathname)==-1){
        r.code=-1;
        sprintf(r.message,"Errore nell'attuazione della politica di rimpiazzamento");
        free(pathname);
        free(content_to_append);
        pthread_mutex_unlock(&mutex_storage);
        pthread_mutex_unlock(&file->mutex_file);
        return r;
    }

    char* new_content=(char*)realloc(file->content,newdim);
    if(!new_content){ //Errore nella realloc
        pthread_mutex_unlock(&mutex_storage);
        pthread_mutex_unlock(&file->mutex_file);
        r.code=-1;
        sprintf(r.message,"Errore nella realloc");
        printf("Errore nella realloc\n");
        free(pathname);
        free(content_to_append);
        LOGFILEAPPEND("[Client %d] Errore nella AppendToFile\n",fd);
        int no_file=0,ctrl;
        SYSCALL(ctrl,write(fd,&no_file,sizeof(int)),"Errore nella scrittura della risposta");
        return r;
    }

    file->content=strncat(new_content,content_to_append,size);
    file->size=(size_t)newdim;
    gettimeofday(&file->last_operation,NULL); 
    pthread_mutex_unlock(&file->mutex_file);

    r.code=0;
    sprintf(r.message,"OK, Il contenuto del file %s e' stato aggiornato dopo la append",pathname);
    LOGFILEAPPEND("[Client %d] Sono stati appesi %d bytes nel file con pathname %s\n",fd,size,pathname);
    
    hash_dump(stdout,storage,print_stored_file_info);

    //Ho terminato le operazioni sullo storage, rilascio il lock
    pthread_mutex_unlock(&mutex_storage);
    free(pathname);
    free(content_to_append);
    return r;
}

response ReadFile(char* pathname,stored_file** found,int fd){
    response r;
    //Verifichiamo se il file e' gia' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    (*found)=hash_find(storage,pathname);
    
    if((*found)==NULL){
        //Il file non e' stato trovato
        fprintf(stderr,"Il file %s non e' stato trovato nello storage\n",pathname);
        r.code=-1;
        sprintf(r.message,"Il file %s non e' stato trovato nello storage",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage, non e' possibile leggerlo\n",fd,pathname);
    }else{ 
        //Il file e' stato trovato, verifichiamo che il contenuto non sia vuoto
        pthread_mutex_lock(&(*found)->mutex_file);

        if(!is_in_list((*found)->opened_by,fd)){
            r.code=-1;
            LOGFILEAPPEND("[Client %d] Non puoi leggere il file %s perche' non e' stato aperto\n",fd,pathname);
            sprintf(r.message,"Il fd %d non ha aperto il file %s",fd,pathname);
            pthread_mutex_unlock(&(*found)->mutex_file);
            pthread_mutex_unlock(&mutex_storage);
            return r;
        }

        if((*found)->content==NULL){
            //Il file e' vuoto!
            r.code=-1;
            sprintf(r.message,"Il contenuto del file %s e' nullo",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s e' vuoto, non e' possibile leggerlo\n",fd,pathname);
        }else{
            //Il file non e' vuoto
            r.code=0;
            sprintf(r.message,"OK, Il file %s e' stato trovato nello storage, ecco il contenuto",pathname);
            LOGFILEAPPEND("[Client %d] Sono stati letti con successo %d bytes del file %s\n",fd,(*found)->size,pathname);
            gettimeofday(&(*found)->last_operation,NULL); 
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    return r;
}

response ReadNFiles(int n,int fd){
    response r;
    entry_t *bucket,*curr;
    
    //Determino il numero di file che saranno inviati dopo aver analizzato 'n'
    int nfiles,ctrl; 
    pthread_mutex_lock(&mutex_storage);
    if(n<=0 || n>not_empty_files_num) {
        //Leggo tutti i files
        nfiles=not_empty_files_num;
    }else{
        //Leggo 'n' files (<= dimensione attuale storage)
        nfiles=n;
    }

    SYSCALL(ctrl,write(fd,&nfiles,sizeof(int)),"Errore nell'invio del numero di file che saranno inviati al server");
    printf("Ho inviato %d bytes per il numero di file %d\n",ctrl,nfiles);
    int saven=nfiles;
    printf("Sono stati chiesti %d files, ne invio %d\n",n,nfiles);
    //Scorrimento della tabella hash
    for(int i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(!nfiles) //Ho letto tutti i file che dovevo
                break;
            if(curr->key){
                stored_file* sf=(stored_file*)curr->data;
                char* id=(char*)curr->key;
                pthread_mutex_lock(&sf->mutex_file);
                if(sf->content){
                    nfiles--;
                    //Invio dimensione del pathname e pathname
                    int pathdim=strlen(id)+1;
                    SYSCALL(ctrl,write(fd,&pathdim,sizeof(int)),"Errore nella 'write' della dimensione del pathname");
                    printf("Ho inviato al client %d bytes di dimensione pathname cioe' %d\n",ctrl,pathdim);
                    SYSCALL(ctrl,write(fd,id,pathdim),"Errore nella 'write' della contenuto del pathname");
                    printf("Ho inviato al client %d bytes di pathname cioe' %s\n",ctrl,id);
                    //Invio dimensione del contenuto e contenuto
                    SYSCALL(ctrl,write(fd,&sf->size,sizeof(int)),"Errore nella 'write' della dimensione del file");
                    printf("Ho inviato al client %d bytes di dimensione contenuto cioe' %d\n",ctrl,(int)sf->size);
                    SYSCALL(ctrl,write(fd,sf->content,(int)sf->size),"Errore nella 'write' della contenuto del file");
                    printf("Ho inviato al client %d bytes di contenuto cioe' %s\n",ctrl,sf->content);
                    gettimeofday(&sf->last_operation,NULL);
                    LOGFILEAPPEND("[Client %d] Sono stati letti con successo %d bytes del file %s\n",fd,sf->size,id);
                }
                pthread_mutex_unlock(&sf->mutex_file);
            }
            curr=curr->next;
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    r.code=0;
    sprintf(r.message,"Sono stati letti con successo %d file dello storage",saven);
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
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage, non lo si puo' sbloccare\n",fd);
    }else{
        pthread_mutex_lock(&found->mutex_file);

        if(!is_in_list(found->opened_by,fd)){
            r.code=-1;
            LOGFILEAPPEND("[Client %d] Non puo' sbloccare il file %s perche' non e' stato aperto\n",fd,pathname);
            sprintf(r.message,"Il fd %d non ha aperto il file %s",fd,pathname);
            pthread_mutex_unlock(&found->mutex_file);
            return r;
        }

        //Il file e' stato trovato
        if(found->fd_holder==fd){
            //Sono il proprietario del file
            found->fd_holder=-1; //annullo il flag
            pthread_cond_signal(&found->is_unlocked); //notifico i client che aspettano
            r.code=0;
            sprintf(r.message,"OK, Il file %s e' stato correttamente sbloccato",pathname);
            LOGFILEAPPEND("[Client %d] Il file %s e' stato correttamente sbloccato\n",fd,pathname);
            gettimeofday(&found->last_operation,NULL); 
        }else{
            //Non sono il proprietario del file
            r.code=-1;
            sprintf(r.message,"Non puoi sbloccare il file %s",pathname);
            LOGFILEAPPEND("[Client %d] Non si dispone dell'autorizzazione per sbloccare il file %s\n",fd,pathname)
        }
        pthread_mutex_unlock(&found->mutex_file);
    }
    return r;
}

int UnlockAllMyFiles(int fd){
    entry_t *bucket, *curr;
    int i;
    if(!storage ||fd<0)
        return -1;
    pthread_mutex_lock(&mutex_storage);
    for(i=0; i<storage->nbuckets; i++) {
        bucket = storage->buckets[i];
        for(curr=bucket; curr!=NULL; ) {
            if(curr->key){
                stored_file* sf=(stored_file*)curr->data;
                pthread_mutex_lock(&sf->mutex_file);
                //printf("File %s Proprietario %d\n",(char*)curr->key,sf->fd_holder);
                if(sf->fd_holder==fd){
                    //Sono il proprietario del file
                    sf->fd_holder=-1; //annullo il flag
                    pthread_mutex_unlock(&sf->mutex_file);
                    pthread_cond_signal(&sf->is_unlocked); //notifico i client che aspettano
                    printf("Ho sbloccato il file %s che era stato bloccato dal fd %d\n",(char*)curr->key,fd);
                    LOGFILEAPPEND("[Client %d] Il file %s e' stato sbloccato perche' e' terminata la connessione\n",fd,(char*)curr->key,fd);
                    gettimeofday(&sf->last_operation,NULL); 
                }else
                    pthread_mutex_unlock(&sf->mutex_file);
            }
            curr=curr->next;
        }
    }
    pthread_mutex_unlock(&mutex_storage);
    return 0;
}

response RemoveFile(char* pathname,int fd){
    response r;
    if(!pathname){
        r.code=-1;
        sprintf(r.message,"Errore parametro nullo");
        LOGFILEAPPEND("[Client %d] Errore RemoveFile\n",fd);
        return r;
    }

    stored_file* found; 
    //Verifichiamo se il file e' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    found=(stored_file*)hash_find(storage,pathname);
    pthread_mutex_unlock(&mutex_storage);

    if(!found){
        r.code=-1;
        sprintf(r.message,"Il file %s non e' presente nello storage",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage, non e' possibile eliminarlo\n",fd,pathname);
        return r;
    }
    pthread_mutex_lock(&found->mutex_file);

    int dim=found->size; //Memorizzo la dimensione del contenuto

    //Verifica dei diritti
    if(found->fd_holder==-1){
        pthread_mutex_unlock(&found->mutex_file);
        //pthread_mutex_unlock(&mutex_storage);
        r.code=-1;
        sprintf(r.message,"Non puoi cancellare un file pubblico");
        LOGFILEAPPEND("[Client %d] Non e' possibile cancellare il file %s in quanto e' pubblico\n",fd,pathname);
        return r;
    }else{
        if (found->fd_holder!=fd || !is_in_list(found->opened_by,fd)){
            r.code=-1;
            sprintf(r.message,"Non hai i diritti per cancellare il file");
            LOGFILEAPPEND("[Client %d] Non hai i diritti per cancellare il file %s\n",fd,pathname);
            pthread_mutex_unlock(&found->mutex_file);
            //pthread_mutex_unlock(&mutex_storage);
            return r;
        }
    }

    //Segnalo che il proprietario vuole eliminare il file
    found->to_delete=1;
    //Nessun client deve aspettare il file che sto per eliminare
    while(found->clients_waiting!=0){
        printf("Sto aspettando che i client che vorranno bloccare il file ci ripensino, al momento stanno aspettando %d client\n",found->clients_waiting);
        pthread_cond_signal(&found->is_unlocked);
        pthread_cond_wait(&found->is_deletable,&found->mutex_file);
    }
    printf("Nessuno sta aspettando questo file,lo elimino\n");

    pthread_mutex_lock(&mutex_storage);
        
    //Procedo all'eliminazione del file
    if(hash_delete(storage,pathname,free,free_stored_file)==-1){
        r.code=-1;
        sprintf(r.message,"Errore nella procedura di eliminazione del file dallo storage");
        LOGFILEAPPEND("[Client %d] Errore RemoveFile\n",fd);
        return r;
    }

    //Aggiorno la dimensione dello storage
    data_size-=dim;
    pthread_mutex_unlock(&mutex_storage);

    r.code=0;
    sprintf(r.message,"OK, Il file %s e' stato correttamente rimosso",pathname);
    LOGFILEAPPEND("[Client %d] Il file %s e' stato correttamente rimosso dallo storage\n",fd,pathname);
    return r;
}

response closeFile(char* pathname,int fd){
    response r;
    if(!pathname){
        r.code=-1;
        sprintf(r.message,"Errore parametro nullo");
        LOGFILEAPPEND("[Client %d] Errore closeFile\n",fd);
        return r;
    }

    stored_file* found; 
    //Verifichiamo se il file e' presente nello storage
    pthread_mutex_lock(&mutex_storage);
    found=(stored_file*)hash_find(storage,pathname);
    pthread_mutex_unlock(&mutex_storage);

    if(!found){
        r.code=-1;
        sprintf(r.message,"Il file %s non e' presente nello storage",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s non e' stato trovato nello storage, non e' possibile chiuderlo\n",fd,pathname);
        return r;
    }

    pthread_mutex_lock(&found->mutex_file);
    if(list_remove(&found->opened_by,fd)==0){
        r.code=-1;
        sprintf(r.message,"Errore nella chiusura del file %s",pathname);
        LOGFILEAPPEND("[Client %d] Errore nella chiusura del file %s\n",fd,pathname);
    }else{
        r.code=0;
        sprintf(r.message,"OK, Il file %s e' stato correttamente chiuso",pathname);
        LOGFILEAPPEND("[Client %d] Il file %s e' stato correttamente chiuso\n",fd,pathname);
    }
    pthread_mutex_unlock(&found->mutex_file);
    hash_dump(stdout,storage,print_stored_file_info);
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
                printf("[EXECUTE REQUEST-OpenFile] Ho ricevuto il flag O_CREATE\n");

            //Leggo il flag o_lock
            int o_lock;
            SYSCALL(ctrl,read(fd,&o_lock,4),"[EXECUTE REQUEST-OpenFile] Errore nella 'read' del flag o_locK");
            if(o_lock)
                printf("[EXECUTE REQUEST-OpenFile] Ho ricevuto il flag O_LOCK\n");

            //Chiamata alla funzione OpenFile che restituisce un oggetto di tipo response che incapsula
            //al suo interno un messaggio ed un codice di errore
            response r=OpenFile(stringbuffer,o_create,o_lock,fd);
            //printf("Ritorno da OpenFile codice %d msg %s\n",r.code,r.message);
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
            response r=ReadFile(stringbuffer,&found,fd);
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
            if(found)
                pthread_mutex_unlock(&found->mutex_file);
            //Rilascio il lock solo dopo aver inviato il contenuto al client
            return r.code;
        }
        case (5):{ //Operazione Read N File
            printf("\n\n*****Operazione READ_N_FILE Fd: %d*****\n",fd);
            int intbuffer; //Buffer di interi
            int ctrl; //variabile che memorizza il risultato di ritorno di chiamate di sistema/funzioni

            //Leggo il numero di file da leggere
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST-ReadNFile] Errore nella 'read' del numero di files");
            
            response r=ReadNFiles(intbuffer,fd);
            
            //-----Invio risposta-----
            int responsedim=strlen(r.message);
            SYSCALL(ctrl,write(fd,&responsedim,sizeof(int)),"Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"Errore nella 'write' della risposta");

            return r.code;
        }
        case (6):{ //Operazione Write File
            printf("\n\n*****Operazione WRITEFILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 6] Errore nella 'read' della dimensione del pathname");
            printf("DEBUG, Ho ricevuto dal client %d bytes di dimensione pathname %d\n",ctrl,intbuffer);
            char* read_name=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(read_name==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_name,intbuffer),"[EXECUTE REQUEST Case 6] Errore nella 'read' del pathname");
            printf("DEBUG, Ho ricevuto dal client %d bytes di pathname %s\n",ctrl,read_name);

            //-----Lettura del file-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 6] Errore nella 'read' della dimensione del file da leggere");
            printf("DEBUG, Ho ricevuto dal client %d bytes di dimensione del contenuto %d\n",ctrl,intbuffer);
            char* read_file=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del file
            if(read_file==NULL){
                perror("Errore nella malloc del file ricevuto");
                free(read_name); //prima di uscire dealloco il pathname gia' letto
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_file,intbuffer),"[EXECUTE REQUEST] Errore nella 'read' del file da leggere");
            printf("DEBUG, Ho ricevuto dal client %d bytes di contenuto\n",ctrl);
            
            //-----Lettura del flag di invio degli eventuali file espulsi
            int flag;
            SYSCALL(ctrl,read(fd,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
            printf("DEBUG, Ho ricevuto dal client %d bytes di flag espulsione %d\n",ctrl,flag);

            //-----Esecuzione operazione
            response r=WriteFile(read_name,read_file,intbuffer,fd,flag);

            //-----Invio risposta-----
            int responsedim=strlen(r.message);
            SYSCALL(ctrl,write(fd,&responsedim,sizeof(int)),"Errore nella 'write' della dimensione della risposta");
            printf("Ho inviato %d bytes di dimensione cioe' %d\n",ctrl,responsedim);
            SYSCALL(ctrl,write(fd,r.message,responsedim),"Errore nella 'write' della risposta");
            printf("Ho inviato %d bytes di stringa: %s\n",ctrl,r.message);
            return r.code;
        }
        case (7):{ //Operazione AppendToFile
            printf("\n\n*****Operazione APPEND TO FILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 7] Errore nella 'read' della dimensione del pathname");
            char* read_name=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(read_name==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_name,intbuffer),"[EXECUTE REQUEST Case 7] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",read_name);

            //-----Lettura del contenuto da appendere-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 7] Errore nella 'read' della dimensione del file da leggere");
            printf("Dimensione file %d\n",intbuffer);
            char* read_file=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del contenuto
            if(read_file==NULL){
                perror("Errore nella malloc del file ricevuto");
                free(read_name); //prima di uscire dealloco il pathname gia' letto
                return -1;
            }
            SYSCALL(ctrl,read(fd,read_file,intbuffer),"[EXECUTE REQUEST Case 7] Errore nella 'read' del file da leggere");
            printf("Ho ricevuto %d bytes\n",ctrl);
            //printf("Ho ricevuto: %s\n",read_file);
            
            //-----Lettura del flag di invio degli eventuali file espulsi
            int flag;
            SYSCALL(ctrl,read(fd,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
            printf("Ho ricevuto il flag %d\n",flag);

            //-----Esecuzione operazione
            response r=AppendToFile(read_name,read_file,intbuffer,fd,flag);
            
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

            response r=LockFile(string_buffer,fd,0);
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
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 9] Errore nella 'read' della dimensione del pathname");
            char* string_buffer=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(string_buffer==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,string_buffer,intbuffer),"[EXECUTE REQUEST Case 9] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",string_buffer);

            response r=UnlockFile(string_buffer,fd);
            free(string_buffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST Case 9] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST Case 9] Errore nella 'write' della risposta");
            return r.code;
        }
        case (10):{ //Operazione closeFile
            printf("\n\n*****Operazione CLOSE FILE Fd: %d*****\n",fd);
            int intbuffer,ctrl;
            //-----Lettura del pathname-----
            SYSCALL(ctrl,read(fd,&intbuffer,sizeof(int)),"[EXECUTE REQUEST Case 10] Errore nella 'read' della dimensione del pathname");
            char* string_buffer=(char*)calloc(intbuffer,sizeof(char)); //Alloco il buffer per la lettura del pathname
            if(string_buffer==NULL){
                perror("Errore malloc buffer pathname");
                return -1;
            }
            SYSCALL(ctrl,read(fd,string_buffer,intbuffer),"[EXECUTE REQUEST Case 10] Errore nella 'read' del pathname");
            printf("Ho ricevuto il pathname %s\n",string_buffer);

            response r=closeFile(string_buffer,fd);
            free(string_buffer);

            int responsedim=strlen(r.message);
            //Invio prima la dimensione della risposta e poi la risposta
            SYSCALL(ctrl,write(fd,&responsedim,4),"[EXECUTE REQUEST Case 10] Errore nella 'write' della dimensione della risposta");
            SYSCALL(ctrl,write(fd,r.message,responsedim),"[EXECUTE REQUEST Case 10] Errore nella 'write' della risposta");
            return r.code;
        }
        case (11):{ //Operazione Remove File
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
            LOGFILEAPPEND("Ho ricevuto il comando sconosciuto %d da parte del client",fun,fd);
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
    //TODO: mutua esclusione sulla lettura del file
    fprintf(stream,"\tSize of content: %d\n\tOwner: %d\n\tCreated: %d.%ld\n\tLast Op: %d.%ld\n\tClients in attesa %d\n\tDa eliminare? %d\n\t",(int)param->size,param->fd_holder,(int)param->creation_time.tv_sec,param->creation_time.tv_usec,(int)param->last_operation.tv_sec,param->last_operation.tv_usec,param->clients_waiting,param->to_delete);
    list_print(param->opened_by);
}

/*Implementazione delle funzioni "readn" e "writen"
(tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)*/
ssize_t  /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount read so far */
     } else if (nread == 0) break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}

/*Implementazione delle funzioni "readn" e "writen"
(tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)*/
ssize_t  /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if((nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount written so far */
     } else if (nwritten == 0) break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
}