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

/*Funzione che apre una connessione AF_UNIX al socket file sockname. Se il server non accetta
immediatamente la richiesta di connessione, si effettua una nuova richiesta dopo msec millisecondi
fino allo scadere del tempo assoluto 'abstime*/
int openConnection(const char* sockname, int msec, const struct timespec abstime){
    //printf ("Provo a connettermi al socket %s\n",sockname);
    //Controllo parametri
    if(sockname==NULL)
        return -1;
    
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
            SYSCALL(ctrl,read(fd_connection,&accepted,sizeof(int)),"Errore nell'int di accettazione di connessione");
            if(accepted){
                IF_PRINT_ENABLED(printf("[openConnection] Il FILE STORAGE ha accettato la connessione!\n");)               
                return EXIT_SUCCESS;
            }else{
                IF_PRINT_ENABLED(fprintf(stderr,"[openConnection] Il FILE STORAGE ha respinto la nostra richiesta di connessione!\n"););
                return -1;
            }
            
        }    
        int timetosleep=msec*1000;
        usleep(timetosleep);
        SYSCALL(err,clock_gettime(CLOCK_REALTIME,&current),"Errore durante la 'clock_gettime");
        //printf("Actual %ld\n",(long)current.tv_sec);
        //printf("Current %ld Start %ld Difference %ld Tolerance %ld\n",current.tv_sec,start.tv_sec,current.tv_sec-start.tv_sec,abstime.tv_sec);
        if((current.tv_sec-start.tv_sec)>=abstime.tv_sec){
            IF_PRINT_ENABLED(fprintf(stderr,"[openConnection] Tempo scaduto per la connessione! (un tentativi ogni %d msec per max %ld sec)\n",msec,abstime.tv_sec););
            return -1;
        }
        //fflush(stdout);
        i++;
    }
    return -1;
}

int closeConnection(const char* sockname){
    //Dobbiamo inviare la richiesta di comando 12 al server
    //Invio il codice comando
    int op=12,ctrl;
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");
    //Chiusura fd della connessione
    if(close(fd_connection)==-1){
        IF_PRINT_ENABLED(fprintf(stderr,"[closeConnection] Errore in chiusura del fd_connection"););
        return -1;
    }
    IF_PRINT_ENABLED(printf("[closeConnection] Chiusura connessione con il FILE STORAGE sul socket %s\n",sockname););
    return EXIT_SUCCESS;
}

int openFile(char* pathname,int o_create,int o_lock){
    //TODO Mettere const char...
    char* path=NULL;
    path=realpath(pathname,path);

    int op=3,dim=strlen(path)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");
    //Invio la dimensione di pathname
    SYSCALL(ctrl,write(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    //Invio la stringa pathname
    SYSCALL(ctrl,write(fd_connection,path,dim),"Errore nella 'write' di pathname");
    free(path);
    //Invio il flag O_CREATE
    SYSCALL(ctrl,write(fd_connection,&o_create,4),"Errore nella 'write' di o_create");
    //Invio il flag O_LOCK
    SYSCALL(ctrl,write(fd_connection,&o_lock,4),"Errore nella 'write' di o_lock");

    //Attendo dimensione della risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    //printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[openFile] Operazione sul file %s fallita\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[openFile] %s\n",response););
    free(response);
    return EXIT_SUCCESS;
}

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

int readFile(char* pathname,void** buf,size_t* size){
    /* TESTING
    if(!pathname || !buf || !size){
        errno=EINVAL;
        perror("Errore parametri nulli");
        return -1;
    }*/

    //TODO Mettere const char...
    char* path=NULL;
    path=realpath(pathname,path);
    
    int op=4,dim=strlen(path)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");

    //Invio la dimensione di pathname e pathname
    SYSCALL(ctrl,write(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,dim),"Errore nella 'write' di pathname");
    

    //Ricevo dimensione della risposta e risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    //printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Operazione sul FILE %s fallita\n",pathname););
        free(path);
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    //printf("[ReadFile]Ho ricevuto dal server %s\n",response);
    IF_PRINT_ENABLED(printf("[readFile] %s\n",response););
    
    //Interpretazione della risposta ed eventuale lettura del contenuto
    //char ok[256];
    //sprintf(ok,"Il file %s e' stato trovato nello storage, ecco il contenuto",path);
    free(path);
    if(strncmp(response,"OK",2)==0){
        //Il file e' stato trovato nello storage, procedo alla lettura del contenuto    
        SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione del contenuto");
        char* content=(char*)calloc(dim,sizeof(char));
        if(content==NULL){
            IF_PRINT_ENABLED(printf("[readFile] Errore nella lettura di %s\n",pathname););
            free(response);
            return -1;
        }
        SYSCALL(ctrl,read(fd_connection,content,dim),"Errore nella 'read' del contenuto");
        IF_PRINT_ENABLED(printf("[readFile] Contenuto di %s: dimensione %d bytes\n %s\n",pathname,dim,content););

        //Se e' stata specificata una cartella di memorizzazione scrivo il file
        if(read_dir){
            //Verifico che dirname sia effettivamente una cartella
            DIR* destination=opendir(read_dir);
            if(!destination){
                if(errno=ENOENT){ //Se la cartella non esiste la creo
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
            //printf("Sono nella cartella %s\n",cwd);

            //Modifico il path del file in modo da salvare tutti i file letti in una unica cartella
            char* save_pathname=strdup(pathname);
            save_pathname=extract_file_name(save_pathname);

            //Scrivo il nuovo file
            FILE* read_file=fopen(save_pathname,"w");
            if(!read_file){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nell'apertura del file letto da memorizzare"););
                free(save_pathname);
                free(response);
                free(content);
                return -1;
            }
            
            //printf("Ho aperto il file!\n");
            int read_bytes=(int)fwrite(content,sizeof(char),ctrl,read_file);
            IF_PRINT_ENABLED(printf("[readFile] Ho scritto %d bytes di file letto\n",read_bytes););
            if(fclose(read_file)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nella chiusura del file"););
                free(save_pathname);
                free(response);
                free(content);
                return -1;
            }
            if(closedir(destination)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readFile] Errore nella chiusura della cartella"););
                free(save_pathname);
                free(response);
                free(content);
                return -1;
            }
            chdir("..");
            memset(cwd,'\0',128);
            getcwd(cwd,sizeof(cwd));
            //printf("Sono nella cartella %s\n",cwd);
            free(save_pathname);
        }

        free(content);
        free(response);
        return EXIT_SUCCESS;
    }
    free(response);
    return -1;
}

int readNFiles(int n, char* dirname){
    //Controllo parametro dirname
    if(!dirname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Non e' stata specificata una cartella per la scrittura dei file letti\n"););
        return -1;
    }

    //Invio al server il codice comando
    int op=5,ctrl;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");
    //printf("Ho inviato %d bytes di codice comando %d\n",ctrl,op);
    //Invio al server il numero di file che richiedo cioe' 'n'
    SYSCALL(ctrl,write(fd_connection,&n,sizeof(int)),"Errore nella 'write' di 'n' in readNFiles");
    //printf("Ho inviato %d bytes di n %d\n",ctrl,n);

    //Attendo dal server il numero di file che ricevero'
    int nfiles;
    SYSCALL(ctrl,read(fd_connection,&nfiles,sizeof(int)),"Errore nella 'write' del codice comando");
    //printf("Ho ricevuto dal server %d bytes di nr file %d\n",ctrl,nfiles);

    DIR* destination=NULL;
    if(read_dir){
            //Verifico che dirname sia effettivamente una cartella
            destination=opendir(read_dir);
            if(!destination){
                if(errno=ENOENT){ //Se la cartella non esiste la creo
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
            char cwd[128];
            getcwd(cwd,sizeof(cwd));
            //printf("Sono nella cartella %s\n",cwd);
    }
    //Ciclo di lettura di 'nfiles' files
    for(int i=0;i<nfiles;i++){
        //printf("@@@@@GIRO %d@@@@@\n",i);
        int dim;
        //Leggo dimensione del pathname e pathname
        SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione del pathname");
        //printf("Dimensione del pathname ricevuta %d per %d\n",ctrl,dim);
        char* pathname=(char*)calloc(dim,sizeof(char));
        if(pathname==NULL){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore malloc\n"););
            return -1;
        }
        SYSCALL(ctrl,read(fd_connection,pathname,dim),"Errore nella 'read' del pathname");

        //Leggo dimensione del contenuto
        SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione del contenuto");
        //printf("Dimensione  ricevuta %d per %d\n",ctrl,dim);
        char* content=(char*)calloc(dim,sizeof(char));
        if(content==NULL){
            IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore malloc"););
            free(pathname);
            return -1;
        }
        SYSCALL(ctrl,read(fd_connection,content,dim),"Errore nella 'read' del contenuto");
        IF_PRINT_ENABLED(printf("[readNFiles] File %d -> Contenuto di %s di dimensione %d bytes\n %s\n",i+1,pathname,dim,content););

        if(read_dir){
            //Scrivo il nuovo file
            pathname=extract_file_name(pathname);
            FILE* read_file=fopen(pathname,"w");
            if(!read_file){
                IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in apertura del file\n"););
                free(pathname);
                free(content);
                return -1;
            }
            IF_PRINT_ENABLED(printf("[readNFiles] Ho scritto %d bytes nel file %s\n",(int)fwrite(content,sizeof(char),ctrl,read_file),pathname););
            if(fclose(read_file)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in chiusura del file\n"););
                free(pathname);
                free(content);
                return -1;
            }
        }
        free(pathname);
        free(content);
    }
    
    if(read_dir){
        if(closedir(destination)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore nella chiusura della cartella\n"););
                return -1;
        }
        chdir("..");
        char cwd[128];
        memset(cwd,'\0',128);
        getcwd(cwd,sizeof(cwd));
        //printf("Sono nella cartella %s\n",cwd);
    }   
    
    //Leggo la dimensione della risposta e la risposta
    int dim;
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[readNFiles] Errore in readNFiles (malloc)\n"););
        perror("Calloc");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("[readNFiles]%s\n",response););
    free(response);
    return nfiles;
}

int writeFile(char* pathname,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore in writefile\n"););
        return -1;
    }

    char* path=NULL;
    path=realpath(pathname,path);

    //Apertura del file
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nell'apertura da inviare al server\n"););
        free(path);
        return -1;
    }
    int size,ctrl;
    //Determinazione della dimensione del file
    struct stat st;
    SYSCALL(ctrl,stat(pathname, &st),"Errore nella 'stat'");
    size = st.st_size;
    int filedim=(int)size; 
    //printf("DIMENSIONE DEL FILE: %d\n",filedim);
    //Alloco un buffer per la lettura del file
    char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
    if(read_file==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
        fclose(to_send);
        free(path);
        return -1;
    }
    //Leggo il file
    //int n=(int)fread(read_file,sizeof(char),filedim,to_send);
    fread(read_file,sizeof(char),filedim,to_send);
    read_file[filedim]='\0';
    //printf("Ho letto %d bytes\n",n);
    //printf("Ho letto %d bytes\nFILE LETTO:\n%s",n,read_file);
    //Chiudo il file
    if(fclose(to_send)==-1){
        IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
        free(path);
        return -1;
    }
    //Invio al server il codice comando
    int op=6; filedim++;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Invio al server il file letto
    SYSCALL(ctrl,write(fd_connection,&filedim,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,write(fd_connection,read_file,filedim),"Errore nella 'write' della dimensione");
    //printf("Ho scritto %d bytes\n",ctrl);
    free(read_file);

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){ //Solo se l'utente ha specificato una directory per la memorizzazione dei file espulsi
        int flag=1;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
        //printf("Ho inviato il flag 1 per dirgli che voglio avere i file espulsi\n");

        //Attendo l'autorizzazione
        int authorized=-1;
        SYSCALL(ctrl,read(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
        //printf("Ho ricevuto il bit di autorizzazione %d\n",authorized);

        if(!authorized){
            IF_PRINT_ENABLED(printf("[writeFile] Non sei autorizzato alla scrittura del file %s!\n",pathname););
            goto read_1;
        }

        int expelled_files_to_send,first_time=1;
        DIR* destination=NULL;
        do{
            SYSCALL(ctrl,read(fd_connection,&expelled_files_to_send,sizeof(int)),"Errore nella 'read' del flag expelled_file");
            //printf("C'e' qualche file da ricevere dal server?%d\n",expelled_files_to_send);

            if(expelled_files_to_send){ //C'e' qualche file da ricevere
                if(first_time){ //E' il primo file espulso, verifico se c'e' gia' la cartella di memorizzazione
                    //Verifico che dirname sia effettivamente una cartella
                    destination=opendir(dirname);
                    if(!destination){
                        if(errno=ENOENT){ //Se la cartella non esiste la creo
                            mkdir(dirname,0777);
                            destination=opendir(dirname);
                            IF_PRINT_ENABLED(printf("[WriteFile] Cartella per i file espulsi non trovata, la creo\n"););
                            if(!destination){
                                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nell'apertura della cartella di destinazione dei file espulsi, i file ricevuti saranno eliminati\n"););
                            }
                        }
                    }
                    //printf("Ho aperto la cartella\n");
                }
                int dim_name,dim_content;

                //-----Lettura del nome del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
                char* buffer_name=(char*)calloc(dim_name,sizeof(char));
                if(!buffer_name){
                    IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
                //printf("Devo ricevere il file con nome %s dim %d\n",buffer_name,dim_name);

                //-----Lettura del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
                char* buffer_content=(char*)calloc(dim_content,sizeof(char));
                if(!buffer_content){
                    free(buffer_name);
                    IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore nella writeFile del file %s\n",pathname););
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");
                IF_PRINT_ENABLED(printf("[writeFile] Ho ricevuto il contenuto del file espulso %s di dim %d \n%s\n",buffer_name,dim_content,buffer_content););

                //Vado nella cartella destinazione
                chdir(dirname);
                char cwd[128];
                getcwd(cwd,sizeof(cwd));
                //printf("Sono nella cartella %s\n",cwd);

                //Scrivo il nuovo file
                buffer_name=extract_file_name(buffer_name);
                FILE* expelled_file=fopen(buffer_name,"w");
                if(!expelled_file){
                    free(buffer_name);
                    free(buffer_content);
                    return -1;
                }
                //printf("Ho aperto il file!\n");
                free(buffer_name);
                IF_PRINT_ENABLED(printf("[writeFile] Ho scritto %d bytes del file espulso nella directory di backup\n",(int)fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file)););
                free(buffer_content);
                if(fclose(expelled_file)==-1){
                    IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] rrore nella chiusura del file\n"););
                    return -1;
                }

                chdir("..");
                memset(cwd,'\0',128);
                getcwd(cwd,sizeof(cwd));
                //printf("Sono nella cartella %s\n",cwd);
            }   
        }while(expelled_files_to_send);

        
        if(destination){
            if(closedir(destination)==-1){
                IF_PRINT_ENABLED(fprintf(stderr,"[writeFile] Errore in chiusura della cartella di destinazione dei file espulsi\n"););
                return -1;
            }
        }
    }else{
        int flag=0;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
    }
   
    //Leggo dal server la dimensione della risposta
    read_1:{
        //printf("ASPETTO UNA RISPOSTA\n");
        int dim=256;
        SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");
        //printf("Dimensione della risposta: %d\n",dim);

        //Alloco un buffer per leggere la risposta del server
        char* response=(char*)calloc(dim+1,sizeof(char));
        if(response==NULL)
            return -1;
        SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
        IF_PRINT_ENABLED(printf("[writeFile] %s\n",response););

        free(response);
        return EXIT_SUCCESS;
    }
}

int lockFile(char* pathname){
    //Controllo parametro pathname
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella lockfile del file %s\n",pathname););
        return -1;
    }

    char* path=NULL;
    path=realpath(pathname,path);

    //Invio al server il codice comando
    int op=8,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella lockfile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("%s\n",response););
    free(response);
    return EXIT_SUCCESS;
}

int unlockFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella unlockfile del file %s\n",pathname););
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);

    //Invio al server il codice comando
    int op=9,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[lockFile] Errore nella unlockfile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("%s\n",response););
    free(response);
    return EXIT_SUCCESS;
}

int removeFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[removeFile] Errore nella removeFile del file %s\n",pathname););
        return -1;
    }
    
    char* path=NULL;
    path=realpath(pathname,path);

    //Invio al server il codice comando
    int op=11,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        IF_PRINT_ENABLED(fprintf(stderr,"[removeFile] Errore nella removeFile del file %s\n",pathname););
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    IF_PRINT_ENABLED(printf("%s\n",response););
    free(response);
    return EXIT_SUCCESS;
}

int appendToFile(char* pathname,void* buf,size_t size,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL || buf==NULL){
        errno=EINVAL;
        IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nella appendToFile del file %s\n",pathname););
        return -1;
    }

    /*  ATTENZIONE, NON BISOGNA APPENDERE UN FILE MA MI SOGNA APPENDERE SIZE BYTES DAL BUFFER PASSATO PER PARAMETRO
    QUINDI RIVEDERE TUTTE QUESTE COSE INUTILI!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    int ctrl;
    
    char* path=NULL;
    path=realpath(pathname,path);

    //Invio al server il codice comando
    int op=7;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(path)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,path,pathdim),"Errore nell'invio del pathname al server");
    free(path);

    //Invio al server il file letto
    int s=(int)size;
    //printf("La dimensione del buffer e' %d\n",s);
    SYSCALL(ctrl,write(fd_connection,&s,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,write(fd_connection,buf,s),"Errore nella 'write' della dimensione");
    //printf("Ho scritto %d bytes\n",ctrl);

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){ //Solo se l'utente ha specificato una directory per la memorizzazione dei file espulsi
        int flag=1;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
        //printf("Ho inviato il flag 1 per dirgli che voglio avere i file espulsi\n");

        //Attendo l'autorizzazione
        int authorized=-1;
        SYSCALL(ctrl,read(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
        //printf("Ho ricevuto il bit di autorizzazione %d\n",authorized);

        if(!authorized){
            //printf("Non sei autorizzato alla scrittura del file %s!\n",pathname);
            IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Non sei autorizzato alla scrittura del file %s!\n",pathname););
            goto read_2;
        }

        //Verifico che dirname sia effettivamente una cartella
        DIR* destination=opendir(dirname);
        if(!destination){
            if(errno=ENOENT){ //Se la cartella non esiste la creo
                mkdir(dirname,0777);
                destination=opendir(dirname);
                //printf("Cartella per i file espulsi non trovata, la creo\n");
                if(!destination){
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'apertura della cartella di destinazione dei file espulsi\n"););
                    return -1;
                }
            }
        }
        //printf("Ho aperto la cartella\n");
        int expelled_files_to_send;
        do{
            SYSCALL(ctrl,read(fd_connection,&expelled_files_to_send,sizeof(int)),"Errore nella 'read' del flag expelled_file");
            //printf("C'e' qualche file da ricevere dal server?%d\n",expelled_files_to_send);

            if(expelled_files_to_send){ //C'e' qualche file da ricevere
                int dim_name,dim_content;

                //-----Lettura del nome del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
                char* buffer_name=(char*)calloc(dim_name,sizeof(char));
                if(!buffer_name){
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
                //printf("Devo ricevere il file con nome %s dim %d\n",buffer_name,dim_name);

                //-----Lettura del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
                char* buffer_content=(char*)calloc(dim_content,sizeof(char));
                if(!buffer_content){
                    free(buffer_name);
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");
                //printf("Ho ricevuto il contenuto %s di dim %d\n",buffer_content,dim_content);

                //Vado nella cartella destinazione
                chdir(dirname);
                char cwd[128];
                getcwd(cwd,sizeof(cwd));
                //printf("Sono nella cartella %s\n",cwd);

                //Scrivo il nuovo file
                buffer_name=extract_file_name(buffer_name);
                FILE* expelled_file=fopen(buffer_name,"w");
                if(!expelled_file){
                    free(buffer_name);
                    free(buffer_content);
                    return -1;
                }
                //printf("Ho aperto il file!");
                free(buffer_name);
                fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file);
                //printf("Ho scritto il file\n");
                free(buffer_content);
                if(fclose(expelled_file)==-1){
                    IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););                  
                    return -1;
                }

                chdir("..");
                memset(cwd,'\0',128);
                getcwd(cwd,sizeof(cwd));
                //printf("Sono nella cartella %s\n",cwd);
            }   
        }while(expelled_files_to_send);

        

        if(closedir(destination)==-1){
            IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] Errore nell'appendToFile del file %s\n",pathname););
            return -1;
        }
    }else{
        int flag=0;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
    }

    read_2:{
        //printf("ASPETTO UNA RISPOSTA\n");
        //Leggo dal server la dimensione della risposta
        int dim=256;
        SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");
        //printf("Dimensione della risposta: %d\n",dim);

        //Alloco un buffer per leggere la risposta del server
        char* response=(char*)calloc(dim+1,sizeof(char));
        if(response==NULL)
            return EXIT_FAILURE;
        SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
        IF_PRINT_ENABLED(fprintf(stderr,"[appendToFile] %s\n",response););

        free(response);
        return 0;
    }
}
