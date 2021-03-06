// created by Simone Schiavone at 20211009 11:36.
// @Università di Pisa
// Matricola 582418
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "serverAPI.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); return(-1);}

/*Implementazione delle funzioni "readn" e "writen"
(tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)*/
ssize_t  /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n) {  
    size_t   nleft;
    ssize_t  nread;
    char* buf=(char*)ptr;
    nleft = n;
    while (nleft > 0) {
        if((nread = read(fd, buf, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount read so far */
        } else if (nread == 0) break; /* EOF */
        nleft -= nread;
        buf+=(int)nread;
    }
    return(n - nleft); /* return >= 0 */
}

/*Implementazione delle funzioni "readn" e "writen"
(tratto da “Advanced Programming In the UNIX Environment” by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley)*/
ssize_t  /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n) {  
    size_t   nleft;
    ssize_t  nwritten;
    char* buf=(char*)ptr;
    nleft = n;
    while (nleft > 0) {
        if((nwritten = write(fd, buf, nleft)) < 0) {
            if (nleft == n) return -1; /* error, return -1 */
            else break; /* error, return amount written so far */
        } else if (nwritten == 0) break; 
        nleft -= nwritten;
        buf   += nwritten;
    }
    return(n - nleft); /* return >= 0 */
}

/*Funzione che apre una connessione AF_UNIX al socket file sockname. Se il server non accetta
immediatamente la richiesta di connessione, si effettua una nuova richiesta dopo 'msec' millisecondi
fino allo scadere del tempo assoluto 'abstime'. Quando la connessione è stata stabilita il server
invia al client un intero di autorizzazione 'accepted'. Se 'accepted' e' uguale a 0 il server ha 
respinto la nostra richiesta di connessione percio' si restituisce un errore. In caso di errore
la funzione restituisce -1, 0 altrimenti.*/
int openConnection(const char* sockname, int msec, const struct timespec abstime){
    //Controllo parametri
    if(sockname==NULL){
        errno=EINVAL;
        return -1;
    }
    
    //Setto l'indirizzo
    struct sockaddr_un sa;
    sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path, sockname, 108);
    SYSCALL(fd_connection,socket(AF_UNIX,SOCK_STREAM,0),"Errore durante la creazione del socket");

    int err;
    struct timespec current,start;
    SYSCALL(err,clock_gettime(CLOCK_REALTIME,&start),"Errore durante la 'clock_gettime'");

    //Connettiamo il socket
    int i=1;
    while(1){
        IF_PRINT_ENABLED(printf("[openConnection] Tentativo di connessione nr %d al socket %s\n",i,sockname););
        if((connect(fd_connection,(struct sockaddr*)&sa,sizeof(sa)))==0){
            IF_PRINT_ENABLED(printf("[openConnection] Connessione con FILE STORAGE correttamente stabilita mediante il socket %s!\n",sockname);)
            int accepted,ctrl;
            SYSCALL(ctrl,readn(fd_connection,&accepted,sizeof(int)),"Errore nell'int di accettazione di connessione");
            if(accepted){
                IF_PRINT_ENABLED(printf("[openConnection] Il FILE STORAGE ha accettato la connessione!\n");)               
                return EXIT_SUCCESS;
            }else{
                IF_PRINT_ENABLED(fprintf(stderr,"[openConnection] Il FILE STORAGE ha respinto la nostra richiesta di connessione!\n"););
                errno=ECONNREFUSED;
                return -1;
            }
            
        }    
        int timetosleep=msec*1000;
        usleep(timetosleep);
        SYSCALL(err,clock_gettime(CLOCK_REALTIME,&current),"Errore durante la 'clock_gettime");
        if((current.tv_sec-start.tv_sec)>=abstime.tv_sec){
            IF_PRINT_ENABLED(fprintf(stderr,"[openConnection] Tempo scaduto per la connessione! (un tentativo ogni %d msec per max %ld sec)\n",msec,abstime.tv_sec););
            errno=EAGAIN;
            return -1;
        }
        i++;
    }
    return -1;
}

/*Funzione che apre una chiude la connessione AF_UNIX al socket file sockname. In caso di errore
la funzione restituisce -1, 0 altrimenti.*/
int closeConnection(const char* sockname){
    //Dobbiamo inviare la richiesta di comando 12 al server
    //Invio il codice comando
    int op=12,ctrl;
    SYSCALL(ctrl,writen(fd_connection,&op,4),"Errore nella 'write' del codice comando");
    //Chiusura fd della connessione
    if(close(fd_connection)==-1){
        IF_PRINT_ENABLED(fprintf(stderr,"[closeConnection] Errore in chiusura del fd_connection"););
        return -1;
    }
    IF_PRINT_ENABLED(printf("[closeConnection] Chiusura connessione con il FILE STORAGE sul socket %s\n",sockname););
    return EXIT_SUCCESS;
}

/*Funzione che apre il file 'pathname' nel server. Se viene specificato il flag 'O_CREATE|O_LOCK il 
file andra' creato nello storage in modalita' locked. Se viene specificato solamente il flag 'O_CREATE'
il file dovra' essere creato nello storage. Se viene specificato solamente il flag 'O_LOCK' il file
viene aperto in modalita' locked. La funzione restituisce 0 se l'operazione e' andata a buon fine, -1 
altrimenti. 
Protocollo:
--> Il client invia al server:
        -il codice comando 3
        -la dimensione del pathname ed il pathname
        -il flag O_CREATE ed il flag O_LOCK
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int openFile(const char* pathname,int flags){
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }
    int op=3,dim=strlen(path)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");
    //Invio la dimensione di pathname
    SYSCALL(ctrl,writen(fd_connection,&dim,sizeof(int)),"Errore nella 'write' della dimensione del pathname");
    //Invio la stringa pathname
    SYSCALL(ctrl,writen(fd_connection,path,dim),"Errore nella 'write' di pathname");
    free(path);

    //Invio il flag O_CREATE
    int int_buf= ((flags==1) || (flags==3)) ? 1 : 0;
    SYSCALL(ctrl,writen(fd_connection,&int_buf,sizeof(int)),"Errore nella 'write' di o_create");

    //Invio il flag O_LOCK
    int_buf= ((flags==2) || (flags==3)) ? 1 : 0;
    SYSCALL(ctrl,writen(fd_connection,&int_buf,sizeof(int)),"Errore nella 'write' di o_lock");

    //Attendo dimensione della risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");
    //printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[openFile] Operazione sul file %s fallita\n",pathname););
        errno=ENOMEM;
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[openFile] %s\n",response););

    //Interpretazione risposta
    if(strncmp(response,"OK,",3)==0){
        free(response);
        return EXIT_SUCCESS;
    }else{
        free(response);
        return -1;
    }
}

/*Procedura di supporto che permette di estrarre l'ultima parte di un path assoluto.
Prende in input il path originale e restituisce un una stringa allocata dinamicamente contenente
il nome del file indirizzato.*/
char* extract_file_name(char* original_path){
	char* tmp;
	char* token=strtok_r(original_path,"/",&tmp);
	char* prec;
	while (token){
        	prec=token;
        	token=strtok_r(NULL,"/",&tmp);
	}
    int dim=strlen(prec)+1; 
    char* extracted=strndup(prec,dim);
    free(original_path);
    return extracted;
}

/*Funzione che permette di leggere il file 'pathname' dal server. Il contenuto del file sara' memorizzato
nel buffer puntato da 'buf' ed il valore puntato da 'size' conterra' la dimensione del contenuto letto.
Se e' stata specificata una cartella per la memorizzazione dei file letti, il file letto sara'
scritto sul disco in tale destinazione. La funzione restituisce 0 in caso di successo, -1 in caso
di fallimento.
Protocollo:
--> Il client invia al server:
        -il codice comando 4
        -la dimensione del pathname ed il pathname
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
        -se la risposta e' positiva allora riceve dimensione del contenuto e contenuto
*/
int readFile(const char* pathname,void** buf,size_t* size){
    if(!pathname || !buf || !size){
        errno=EINVAL;
        perror("Errore parametri nulli");
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    int op=4,dim=strlen(path)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,writen(fd_connection,&op,4),"Errore nella 'write' del codice comando");

    //Invio la dimensione di pathname e pathname
    SYSCALL(ctrl,writen(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,dim),"Errore nella 'write' di pathname");
    

    //Ricevo dimensione della risposta e risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Operazione sul FILE %s fallita\n",pathname););
        errno=ENOMEM;
        free(path);
        return -1;
    }
    free(path);
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[readFile] %s\n",response););

    //Interpretazione della risposta ed eventuale lettura del contenuto
    if(strncmp(response,"OK,",3)==0){
        //Il file e' stato trovato nello storage, procedo alla lettura del contenuto    
        SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione del contenuto");
        (*buf)=(char*)calloc(dim,sizeof(char));
        if((*buf)==NULL){
            IF_PRINT_ENABLED(printf("[readFile] Errore nella lettura di %s\n",pathname););
            errno=ENOMEM;
            free(response);
            return -1;
        }
        SYSCALL(ctrl,readn(fd_connection,(*buf),dim),"Errore nella 'read' del contenuto");
        IF_PRINT_ENABLED(printf("[readFile] Contenuto di %s: dimensione %d bytes\n %s\n",pathname,dim,(char*)*buf););

        *size=(size_t)dim;

        //Se e' stata specificata una cartella di memorizzazione scrivo il file
        if(read_dir){
            //Verifico che dirname sia effettivamente una cartella
            DIR* destination=opendir(read_dir);
            if(!destination){
                if(errno==ENOENT){ //Se la cartella non esiste la creo
                    mkdir(read_dir,0777);
                    destination=opendir(read_dir);
                    if(!destination){
                        IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nell'apertura della cartella dei file letti, i file letti non saranno memorizzati"););
                    }else{
                         IF_PRINT_ENABLED(printf("[readFile] Cartella per i file letti non esistente, la creo\n"););
                    }
                }
            }
            //Vado nella cartella destinazione
            chdir(read_dir);
            char cwd[128];
            getcwd(cwd,sizeof(cwd));

            //Modifico il path del file in modo da salvare tutti i file letti in una unica cartella
            char* save_pathname=strdup(pathname);
            save_pathname=extract_file_name(save_pathname);

            //Scrivo il nuovo file
            FILE* read_file=fopen(save_pathname,"w");
            if(!read_file){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nell'apertura del file letto da memorizzare"););
                free(save_pathname);
                free(response);
                free(buf);
                (*buf)=NULL;
                return -1;
            }
            
            int read_bytes=(int)fwrite((*buf),sizeof(char),ctrl-1,read_file);
            IF_PRINT_ENABLED(printf("[readFile] Ho scritto %d bytes di file letto\n",read_bytes););
            if(fclose(read_file)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nella chiusura del file"););
                free(save_pathname);
                free(response);
                free(buf);
                (*buf)=NULL;
                return -1;
            }
            if(closedir(destination)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nella chiusura della cartella"););
                free(save_pathname);
                free(response);
                free(buf);
                (*buf)=NULL;
                return -1;
            }
            chdir(working_directory);
            free(save_pathname);
        }

        free(response);
        return EXIT_SUCCESS;
    }
    free(response);
    return -1;
}

/*Funzione che permette di leggere al piu' 'n' file dal server.I file letti saranno scritti sul disco
nella cartella specificata dall'opzione -d. Se 'n' e' uguale a zero viene richiesta al server la lettura
di tutti i file contenuti nello storage. La funzione restituisce il numero di file letti
in caso di successo, -1 in caso di fallimento.
Protocollo:
--> Il client invia al server:
        -il codice comando 5
        -il numero di file richiesti ('n')
<-- Il client riceve dal server:
        -il numero di file che saranno ricevuti ('nfiles')
        -per ogni file che deve essere letto ricevera':
            -dimensione del pathname e pathname
            -dimensione del contenuto e contenuto
        -dimensione della risposta e risposta
*/
int readNFiles(int n, char* dirname){
    //Controllo parametro dirname
    if(!dirname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Non e' stata specificata una cartella per la scrittura dei file letti\n"););
        return -1;
    }

    //Invio al server il codice comando
    int op=5,ctrl;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");
    printf("Ho inviato %d bytes di codice comando %d\n",ctrl,op);
    //Invio al server il numero di file che richiedo cioe' 'n'
    SYSCALL(ctrl,writen(fd_connection,&n,sizeof(int)),"Errore nella 'write' di 'n' in readNFiles");
    printf("Ho inviato %d bytes di n %d\n",ctrl,n);

    //Attendo dal server il numero di file che ricevero'
    int nfiles;
    SYSCALL(ctrl,readn(fd_connection,&nfiles,sizeof(int)),"Errore nella 'read' del codice comando");
    printf("Ho ricevuto dal server %d bytes di nr file %d\n",ctrl,nfiles);

    DIR* destination=NULL;
    //Verifico che dirname sia effettivamente una cartella
    destination=opendir(read_dir);
    if(!destination){
        if(errno==ENOENT){ //Se la cartella non esiste la creo
            mkdir(read_dir,0777);
            destination=opendir(read_dir);
            IF_PRINT_ENABLED(printf("[readNFiles] Cartella per i file letti non trovata, la creo\n"););
            if(!destination){
                IF_PRINT_ENABLED(fprintf(stderr,"Errore nell'apertura della cartella di destinazione dei file letti, i file letti non saranno memorizzati\n"););
            }
        }
    }
    //Vado nella cartella destinazione
    chdir(read_dir);

    //Ciclo di lettura di 'nfiles' files
    for(int i=0;i<nfiles;i++){
        int dim;
        //Leggo dimensione del pathname e pathname
        SYSCALL(ctrl,readn(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione del pathname");
        char* pathname=(char*)calloc(dim,sizeof(char));
        if(pathname==NULL){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore malloc\n"););
            return -1;
        }
        SYSCALL(ctrl,readn(fd_connection,pathname,dim),"Errore nella 'read' del pathname");
        //printf("Dimensione del pathname ricevuta %d per %s\n",dim,pathname);

        //Leggo dimensione del contenuto
        SYSCALL(ctrl,readn(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione del contenuto");
        char* content=(char*)calloc(dim,sizeof(char));
        if(content==NULL){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore malloc"););
            free(pathname);
            return -1;
        }
        SYSCALL(ctrl,readn(fd_connection,content,dim),"Errore nella 'read' del contenuto");
        //printf("Dimensione del contenuto ricevuta %d per %d\n",ctrl,dim);
        IF_PRINT_ENABLED(printf("[readNFiles] File %d -> Contenuto di %s di dimensione %d bytes\n %s\n",i+1,pathname,dim,content););

        //Scrivo il nuovo file
        pathname=extract_file_name(pathname);
        FILE* read_file=fopen(pathname,"w");
        if(!read_file){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in apertura del file\n"););
            free(pathname);
            free(content);
            return -1;
        }
        int k=(int)fwrite(content,sizeof(char),ctrl-1,read_file);
        IF_PRINT_ENABLED(printf("[readNFiles] Ho scritto %d bytes nel file %s\n",k,pathname););
        if(fclose(read_file)==-1){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in chiusura del file\n"););
            free(pathname);
            free(content);
            return -1;
        }    
        free(pathname);
        free(content);
    }
    
    if(closedir(destination)==-1){
        IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore nella chiusura della cartella\n"););
        return -1;
    }
    chdir(working_directory); 
    
    //Leggo la dimensione della risposta e la risposta
    int dim;
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in readNFiles (malloc)\n"););
        perror("Calloc");
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[readNFiles] %s\n",response););
    free(response);
    return nfiles;
}

/*Funzione che permette di scrivere il contenuto del file 'pathname' sullo storage.
La funzione restituisce 0 in caso di successo e -1 in caso di fallimento. Il parametro 'dirname'
se non e' nullo specifica il nome della cartella dove memorizzare i file eventualmente espulsi dal 
server in conseguenza a capacity misses. 
Protocollo:
--> Il client invia al server:
        -il codice comando 6
        -dimensione del pathname e pathname del file da scrivere
        -dimensione del contenuto e contenuto del file da scrivere
        -flag di invio di file espulsi (1 se 'dirname' non e' nullo, 0 altrimenti)
<-- Il client riceve dal server:
        -flag di autorizzazione a procedere; se 0 abortiamo l'operazione

Finche' c'e' qualche file espulso dal server da recuperare:[
    <-- Il client riceve dal server:
            -flag di ricezione di nuovo file espulso; se 0 usciamo dal "ciclo" di ricezione
            -dimensione del pathname e pathname del file espulso
            -dimensione del contenuto e contenuto del file espulso
]

<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int writeFile(char* pathname,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore in writefile\n"););
        return -1;
    }

    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }
    
    //Apertura del file
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nell'apertura da inviare al server\n"););
        free(path);
        return -1;
    }
    int ctrl;

    //Alloco un buffer per la lettura del file
    fseek(to_send,0,SEEK_END);
    int filedim=ftell(to_send);
    char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
    if(read_file==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
        fclose(to_send);
        free(path);
        return -1;
    }
    //Leggo il file
    fseek(to_send,0,0);
    fread(read_file,sizeof(char),filedim,to_send);
    read_file[filedim]='\0';

    //Chiudo il file
    if(fclose(to_send)==-1){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
        free(path);
        return -1;
    }

    //Invio al server il codice comando
    int op=6; filedim++;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Invio al server il file letto
    SYSCALL(ctrl,writen(fd_connection,&filedim,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,writen(fd_connection,read_file,filedim),"Errore nella 'write' della dimensione");
    free(read_file);

    //Invio flag per ricevere i file eventualmente espulsi dall'algoritmo di rimpiazzamento
    //nel caso l'utente abbia specificato una directory per la memorizzazione
    int flag=(dirname==NULL) ? 0 : 1;
    SYSCALL(ctrl,writen(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
    
    //Attendo l'autorizzazione
    int authorized=-1;
    SYSCALL(ctrl,readn(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
    if(!authorized){
        IF_PRINT_ENABLED(printf("[writeFile] Non sei autorizzato alla scrittura del file %s!\n",pathname););
        goto read_1;
    }

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){ 
        int expelled_file_to_receive;
        DIR* destination=NULL;
        do{ 
            //Verifico se c'e' qualche file da ricevere
            SYSCALL(ctrl,readn(fd_connection,&expelled_file_to_receive,sizeof(int)),"Errore nella 'readn' del flag expelled_file");
            if(expelled_file_to_receive==0){ //se non ci sono file da ricevere vado a leggere la risposta del server
                //printf("Debug, non devo ricevere ulteriori file espulsi, proseguo alla lettura della risposta\n");
                goto read_1;
            }
                
            if(!destination){
                if(errno==ENOENT){ //Se la cartella non esiste la creo
                    mkdir(dirname,0777);
                    destination=opendir(dirname);
                    IF_PRINT_ENABLED(printf("[WriteFile] Cartella per i file espulsi non trovata, la creo\n"););
                    if(!destination){
                        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nell'apertura della cartella di destinazione dei file espulsi, i file ricevuti saranno eliminati\n"););
                    }
                }
            }
                
            int dim_name,dim_content;

            //-----Lettura del nome del file espulso
            SYSCALL(ctrl,readn(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
            char* buffer_name=(char*)calloc(dim_name,sizeof(char));
            if(!buffer_name){
                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
                return -1;
            }
            SYSCALL(ctrl,readn(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
            
            //-----Lettura del file espulso
            SYSCALL(ctrl,readn(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
            char* buffer_content=(char*)calloc(dim_content,sizeof(char));
            if(!buffer_content){
                free(buffer_name);
                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
                return -1;
            }
            SYSCALL(ctrl,readn(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");
            IF_PRINT_ENABLED(printf("[writeFile] Ho ricevuto il contenuto del file espulso %s di dim %d \n%s\n",buffer_name,dim_content,buffer_content););

            //Vado nella cartella destinazione
            chdir(dirname);

            //Scrivo il nuovo file
            buffer_name=extract_file_name(buffer_name);
            FILE* expelled_file=fopen(buffer_name,"w");
            if(!expelled_file){
                free(buffer_name);
                free(buffer_content);
                chdir(working_directory);
                return -1;
            }
            free(buffer_name);
            int k=(int)fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file);
            IF_PRINT_ENABLED(printf("[writeFile] Ho scritto %d bytes del file espulso nella directory di backup\n",k););
            free(buffer_content);
            if(fclose(expelled_file)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella chiusura del file\n"););
                chdir(working_directory);
                return -1;
            }

            if(destination){
                if(closedir(destination)==-1){
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nella 'WriteFile' del file %s\n",pathname););
                }
            }
            chdir(working_directory);
        }while(expelled_file_to_receive);

        
        if(destination){
            if(closedir(destination)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore in chiusura della cartella di destinazione dei file espulsi\n"););
                return -1;
            }
        }
    }
   
    //Leggo dal server la dimensione della risposta
    read_1:{
        int dim=256;
        SYSCALL(ctrl,readn(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");

        //Alloco un buffer per leggere la risposta del server
        char* response=(char*)calloc(dim+1,sizeof(char));
        if(response==NULL)
            return -1;
        SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
        IF_PRINT_ENABLED(printf("[writeFile] %s\n",response););

        //Interpretazione risposta
        if(strncmp(response,"OK,",3)==0){
            free(response);
            return EXIT_SUCCESS;
        }else{
            free(response);
            return -1;
        }
    }
}

/*Funzione che permette di bloccare il file 'pathname'. Restituisce -1 in caso di errore, 0
altrimenti.
Protocollo:
--> Il client invia al server:
        -il codice comando 8
        -dimensione del pathname e pathname del file da bloccare
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int lockFile(char* pathname){
    //Controllo parametro pathname
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella lockfile del file %s\n",pathname););
        return -1;
    }

    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    //Invio al server il codice comando
    int op=8,ctrl,dim;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella lockfile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[lockFile] %s\n",response););

    //Interpretazione risposta
    if(strncmp(response,"OK,",3)==0){
        free(response);
        return EXIT_SUCCESS;
    }else{
        free(response);
        return -1;
    }
}

/*Funzione che permette di sbloccare il file 'pathname'. Restituisce -1 in caso di errore, 0
altrimenti.
Protocollo:
--> Il client invia al server:
        -il codice comando 9
        -dimensione del pathname e pathname del file da sbloccare
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int unlockFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella unlockfile del file %s\n",pathname););
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    //Invio al server il codice comando
    int op=9,ctrl,dim;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella unlockfile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[unlockFile] %s\n",response););
    
    //Interpretazione risposta
    if(strncmp(response,"OK,",3)==0){
        free(response);
        return EXIT_SUCCESS;
    }else{
        free(response);
        return -1;
    }
}

/*Funzione che permette di rimuovere il file 'pathname' dallo storage.
Restituisce -1 in caso di errore, 0 altrimenti.
Protocollo:
--> Il client invia al server:
        -il codice comando 11
        -dimensione del pathname e pathname del file da rimuovere
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int removeFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[removeFile] Errore nella removeFile del file %s\n",pathname););
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    //Invio al server il codice comando
    int op=11,ctrl,dim;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[removeFile] Errore nella removeFile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[removeFile] %s\n",response););

    //Interpretazione risposta
    if(strncmp(response,"OK,",3)==0){
        free(response);
        return EXIT_SUCCESS;
    }else{
        free(response);
        return -1;
    }
}

/*Funzione che permette di appendere del contenuto al file 'pathname' sullo storage.
La funzione restituisce 0 in caso di successo e -1 in caso di fallimento. Il parametro 'dirname'
se non e' nullo specifica il nome della cartella dove memorizzare i file eventualmente espulsi dal 
server in conseguenza a capacity misses. Il parametro 'buf' contiene un riferimento al buffer
del contenuto da appendere, il parametro 'size' contiene la dimensione del contenuto da appendere.
Protocollo:
--> Il client invia al server:
        -il codice comando 7
        -dimensione del pathname e pathname del file a cui appendere del contenuto
        -dimensione del contenuto e contenuto da appendere al file
        -flag di invio di file espulsi (1 se 'dirname' non e' nullo, 0 altrimenti)
<-- Il client riceve dal server:
        -flag di autorizzazione a procedere; se 0 abortiamo l'operazione

Finche' c'e' qualche file espulso dal server da recuperare:[
    <-- Il client riceve dal server:
            -flag di ricezione di nuovo file espulso; se 0 usciamo dal "ciclo" di ricezione
            -dimensione del pathname e pathname del file espulso
            -dimensione del contenuto e contenuto del file espulso
]

<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int appendToFile(char* pathname,void* buf,size_t size,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL || buf==NULL){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nella appendToFile del file %s\n",pathname););
        return -1;
    }

    int ctrl;
    
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    //Invio al server il codice comando
    int op=7;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Invio al server il file letto
    int s=(int)size;
    SYSCALL(ctrl,writen(fd_connection,&s,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,writen(fd_connection,buf,s),"Errore nella 'write' della dimensione");
    free(buf);
    
    //Invio flag per ricevere i file eventualmente espulsi dall'algoritmo di rimpiazzamento
    //nel caso l'utente abbia specificato una directory per la memorizzazione
    int flag=(dirname==NULL) ? 0 : 1;
    SYSCALL(ctrl,writen(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
    
    //Attendo l'autorizzazione
    int authorized=-1;
    SYSCALL(ctrl,readn(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
    if(!authorized){
        IF_PRINT_ENABLED(printf("[appendToFile] Non sei autorizzato alla scrittura del file %s!\n",pathname););
        goto read_2;
    }

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){
        int expelled_file_to_receive; 
    
        do{
            //Verifico se c'e' qualche file da ricevere
            SYSCALL(ctrl,readn(fd_connection,&expelled_file_to_receive,sizeof(int)),"Errore nella 'readn' del flag expelled_file");
            if(expelled_file_to_receive==0){ //se non ci sono file da ricevere vado a leggere la risposta del server
                //printf("Debug, non devo ricevere ulteriori file espulsi, proseguo alla lettura della risposta\n");
                goto read_2;
            }
            
            //Verifico che dirname sia effettivamente una cartella
            DIR* destination=NULL;
            destination=opendir(dirname);
            if(!destination){
                if(errno==ENOENT){ //Se la cartella non esiste la creo
                    mkdir(dirname,0777);
                    destination=opendir(dirname);
                    IF_PRINT_ENABLED(printf("[appendToFile] Cartella per i file espulsi non trovata, la creo\n"););
                    if(!destination){
                        IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'apertura della cartella di destinazione dei file espulsi, i file ricevuti saranno eliminati\n"););
                    }
                }
            }

            
            int dim_name=0,dim_content=0;

            //-----Lettura del nome del file espulso
            SYSCALL(ctrl,readn(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
            char* buffer_name=(char*)calloc(dim_name,sizeof(char));
            if(!buffer_name){
                IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
                return -1;
            }
            SYSCALL(ctrl,readn(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
            //printf("Debug, ho letto %d byte di nome: devo ricevere il file espulso %s\n",ctrl,buffer_name);

            //-----Lettura del file espulso
            SYSCALL(ctrl,readn(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
            char* buffer_content=(char*)calloc(dim_content,sizeof(char));
            if(!buffer_content){
                free(buffer_name);
                IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
                return -1;
            }
            SYSCALL(ctrl,readn(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");

            //Vado nella cartella destinazione
            chdir(dirname); 

            //Scrivo il nuovo file
            buffer_name=extract_file_name(buffer_name);
            FILE* expelled_file=fopen(buffer_name,"w");
            if(!expelled_file){
                free(buffer_name);
                free(buffer_content);
                return -1;
            }
            free(buffer_name);
            int k=(int)fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file);
            IF_PRINT_ENABLED(printf("[appendToFile] Ho scritto %d bytes del file espulso nella directory di backup\n",k););
            free(buffer_content);
            if(fclose(expelled_file)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););                  
                return -1;
            }
            
            if(destination){
                if(closedir(destination)==-1){
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
                    return -1;
                }
                printf("Destination CHIUSO");
            }

            chdir(working_directory);
        }while(expelled_file_to_receive);

        
    }

    read_2:{
        //Leggo dal server la dimensione della risposta
        int dim=256;
        SYSCALL(ctrl,readn(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");

        //Alloco un buffer per leggere la risposta del server
        char* response=(char*)calloc(dim+1,sizeof(char));
        if(response==NULL)
            return -1;
        SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
        IF_PRINT_ENABLED(printf("[appendToFile] %s\n",response););

        //Interpretazione risposta
        if(strncmp(response,"OK,",3)==0){
            free(response);
            return EXIT_SUCCESS;
        }else{
            free(response);
            return -1;
        }
    }
}

/*Funzione che permette la chiusura del file 'pathname'. 
Protocollo:
--> Il client invia al server:
        -il codice comando 10
        -la dimensione del pathname ed il pathname del file da chiudere
<-- Il client riceve dal server:
        -dimensione della risposta e risposta
*/
int closeFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[closeFile] Errore nella closeFile del file %s\n",pathname););
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);
    if(path==NULL){
        if(errno==ENOENT)
            IF_PRINT_ENABLED(printf("Il file %s non esiste\n",pathname););
        return -1;
    }

    //Invio al server il codice comando
    int op=10,ctrl,dim;
    SYSCALL(ctrl,writen(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,writen(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,writen(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,readn(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[closeFile] Errore nella closeFile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,readn(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[closeFile] %s\n",response););

    //Interpretazione risposta
    if(strncmp(response,"OK,",3)==0){
        free(response);
        return EXIT_SUCCESS;
    }else{
        free(response);
        return -1;
    }
}
