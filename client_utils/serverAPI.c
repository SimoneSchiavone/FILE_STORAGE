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
    printf ("Provo a connettermi al socket %s\n",sockname);
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
    printf("Inizio %ld\n",(long)start.tv_sec);

    //Connettiamo il socket
    while(1){
        printf("Provo a connettermi...\n");
        if((connect(fd_connection,(struct sockaddr*)&sa,sizeof(sa)))==-1){
            perror("Errore nella connect");
        }else{
            printf("Connesso correttamente!\n");
            return EXIT_SUCCESS;
        }    
        int timetosleep=msec*1000;
        usleep(timetosleep);
        SYSCALL(err,clock_gettime(CLOCK_REALTIME,&current),"Errore durante la 'clock_gettime");
        //printf("Actual %ld\n",(long)current.tv_sec);
        //printf("Current %ld Start %ld Difference %ld Tolerance %ld\n",current.tv_sec,start.tv_sec,current.tv_sec-start.tv_sec,abstime.tv_sec);
        if((current.tv_sec-start.tv_sec)>=abstime.tv_sec){
            fprintf(stderr,"Tempo scaduto per la connessione!\n");
            return -1;
        }
        fflush(stdout);
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
        perror("Errore in chiusura del fd_connection");
        return -1;
    }
    return EXIT_SUCCESS;
}

int openFile(char* pathname,int o_create,int o_lock){
    //TODO Mettere const char...

    int op=3,dim=strlen(pathname)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");
    //Invio la dimensione di pathname
    SYSCALL(ctrl,write(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    //Invio la stringa pathname
    SYSCALL(ctrl,write(fd_connection,pathname,dim),"Errore nella 'write' di pathname");
    //Invio il flag O_CREATE
    SYSCALL(ctrl,write(fd_connection,&o_create,4),"Errore nella 'write' di o_create");
    //Invio il flag O_LOCK
    SYSCALL(ctrl,write(fd_connection,&o_lock,4),"Errore nella 'write' di o_lock");

    //Attendo dimensione della risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Malloc");
        return EXIT_FAILURE;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("[OpenFile]Ho ricevuto dal server %s\n",response);
    free(response);
    /*
    SYSCALL(n,read(fd_connection,buffer,BUFFDIM),"Errore nella read");
    printf("Ho ricevuto %s\n",buffer);
    memset(buffer,'\0',BUFFDIM);
    SYSCALL(n,write(fd_connection,"stop",4),"Errore nella write");
    SYSCALL(n,read(fd_connection,buffer,BUFFDIM),"Errore nella read");
    printf("Ho ricevuto %s\n",buffer);
    free(buffer);
    close(fd_connection);*/
    return EXIT_SUCCESS;
}

int readFile(char* pathname,void** buf,size_t* size){
    /*
    if(!pathname || !buf || !size){
        errno=EINVAL;
        perror("Errore parametri nulli");
        return -1;
    }*/
    //TODO Mettere const char...
    int op=4,dim=strlen(pathname)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");

    //Invio la dimensione di pathname e pathname
    SYSCALL(ctrl,write(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,dim),"Errore nella 'write' di pathname");

    //Ricevo dimensione della risposta e risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Malloc response");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("[ReadFile]Ho ricevuto dal server %s\n",response);
    
    //Interpretazione della risposta ed eventuale lettura del contenuto
    char ok[100];
    sprintf(ok,"Il file %s e' stato trovato nello storage, ecco il contenuto",pathname);
    if(strncmp(response,ok,strlen(ok))==0){
        //Il file e' stato trovato nello storage, procedo alla lettura del contenuto    
        SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione del contenuto");
        char* content=(char*)calloc(dim,sizeof(char));
        if(content==NULL){
            perror("Malloc content");
            free(response);
            return -1;
        }
        SYSCALL(ctrl,read(fd_connection,content,dim),"Errore nella 'read' del contenuto");
        printf("Contenuto di %s\n %s\n",pathname,content);
        free(content);
        free(response);
        return 0;
    }
    free(response);
    return -1;
}

int writeFile(char* pathname,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }

    //Apertura del file
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        perror("Errore nell'apertura da inviare al server");
        return -1;
    }
    int size,ctrl;
    //Determinazione della dimensione del file
    struct stat st;
    SYSCALL(ctrl,stat(pathname, &st),"Errore nella 'stat'");
    size = st.st_size;
    int filedim=(int)size; 
    printf("DIMENSIONE DEL FILE: %d\n",filedim);
    //Alloco un buffer per la lettura del file
    char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
    if(read_file==NULL){
        perror("Calloc buffer lettura file da inviare");
        fclose(to_send);
        return -1;
    }
    //Leggo il file
    int n=(int)fread(read_file,sizeof(char),filedim,to_send);
    read_file[filedim]='\0';
    printf("Ho letto %d bytes\n",n);
    //printf("Ho letto %d bytes\nFILE LETTO:\n%s",n,read_file);
    //Chiudo il file
    if(fclose(to_send)==-1){
        perror("Errore fclose file da inviare");
        return -1;
    }
    //Invio al server il codice comando
    int op=6; filedim++;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(pathname)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,pathdim),"Errore nell'invio del pathname al server");

    //Invio al server il file letto
    SYSCALL(ctrl,write(fd_connection,&filedim,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,write(fd_connection,read_file,filedim),"Errore nella 'write' della dimensione");
    printf("Ho scritto %d bytes\n",ctrl);
    free(read_file);

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){ //Solo se l'utente ha specificato una directory per la memorizzazione dei file espulsi
        int flag=1;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
        printf("Ho inviato il flag 1 per dirgli che voglio avere i file espulsi\n");

        //Attendo l'autorizzazione
        int authorized=-1;
        SYSCALL(ctrl,read(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
        printf("Ho ricevuto il bit di autorizzazione %d\n",authorized);

        if(!authorized){
            printf("Non sei autorizzato alla scrittura del file %s!\n",pathname);
            return -1;
        }

        //Verifico che dirname sia effettivamente una cartella
        DIR* destination=opendir(dirname);
        if(!destination){
            if(errno=ENOENT){ //Se la cartella non esiste la creo
                mkdir(dirname,0777);
                destination=opendir(dirname);
                printf("Cartella per i file espulsi non trovata, la creo\n");
                if(!destination){
                    perror("Errore nell'apertura della cartella di destinazione dei file espulsi");
                    return -1;
                }
            }
        }
        printf("Ho aperto la cartella\n");
        int expelled_files_to_send;
        do{
            SYSCALL(ctrl,read(fd_connection,&expelled_files_to_send,sizeof(int)),"Errore nella 'read' del flag expelled_file");
            printf("C'e' qualche file da ricevere dal server?%d\n",expelled_files_to_send);

            if(expelled_files_to_send){ //C'e' qualche file da ricevere
                int dim_name,dim_content;

                //-----Lettura del nome del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
                char* buffer_name=(char*)calloc(dim_name,sizeof(char));
                if(!buffer_name){
                    perror("Calloc");
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
                printf("Devo ricevere il file con nome %s dim %d\n",buffer_name,dim_name);

                //-----Lettura del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
                char* buffer_content=(char*)calloc(dim_content,sizeof(char));
                if(!buffer_content){
                    free(buffer_name);
                    perror("Calloc");
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");
                printf("Ho ricevuto il contenuto %s di dim %d\n",buffer_content,dim_content);

                //Vado nella cartella destinazione
                chdir(dirname);
                char cwd[128];
                getcwd(cwd,sizeof(cwd));
                printf("Sono nella cartella %s\n",cwd);

                //Scrivo il nuovo file
                FILE* expelled_file=fopen(buffer_name,"w");
                if(!expelled_file){
                    free(buffer_name);
                    free(buffer_content);
                    return -1;
                }
                printf("Ho aperto il file!");
                free(buffer_name);
                fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file);
                printf("Ho scritto il file\n");
                free(buffer_content);
                if(fclose(expelled_file)==-1){
                    perror("Errore nella chiusura del file");
                    return -1;
                }

                chdir("..");
                memset(cwd,'\0',128);
                getcwd(cwd,sizeof(cwd));
                printf("Sono nella cartella %s\n",cwd);
            }   
        }while(expelled_files_to_send);

        

        if(closedir(destination)==-1){
            perror("Errore in chiusura della cartella di destinazione dei file espulsi");
            return -1;
        }
    }else{
        int flag=0;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
    }
   
    //Leggo dal server la dimensione della risposta
    int dim=256;
    printf("ASPETTO UNA RISPOSTA\n");
    SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");
    printf("Dimensione della risposta: %d\n",dim);

    //Alloco un buffer per leggere la risposta del server
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL)
        return EXIT_FAILURE;
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("Ho ricevuto dal server: %s\n",response);

    free(response);
    return 0;
}

int lockFile(char* pathname){
    //Controllo parametro pathname
    if(!pathname){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }

    //Invio al server il codice comando
    int op=8,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(pathname)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,pathdim),"Errore nell'invio del pathname al server");

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Calloc");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("%s\n",response);
    free(response);
    return EXIT_SUCCESS;
}

int unlockFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }
    
    //Invio al server il codice comando
    int op=9,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(pathname)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,pathdim),"Errore nell'invio del pathname al server");

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Calloc");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("%s\n",response);
    free(response);
    return EXIT_SUCCESS;
}

int removeFile(char* pathname){
    if(!pathname){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }
    
    //Invio al server il codice comando
    int op=11,ctrl,dim;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(pathname)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,pathdim),"Errore nell'invio del pathname al server");

    //Leggo la dimensione della risposta e la risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Calloc");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("%s\n",response);
    free(response);
    return EXIT_SUCCESS;
}

int appendToFile(char* pathname,void* buf,size_t size,char* dirname){
    //Controllo parametro pathname
    if(pathname==NULL || buf==NULL){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }

    //Apertura del file
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        perror("Errore nell'apertura da inviare al server");
        return -1;
    }
    int size,ctrl;
    //Determinazione della dimensione del file
    struct stat st;
    SYSCALL(ctrl,stat(pathname, &st),"Errore nella 'stat'");
    size = st.st_size;
    int filedim=(int)size; 
    printf("DIMENSIONE DEL FILE: %d\n",filedim);
    //Alloco un buffer per la lettura del file
    char* read_file=(char*)calloc(filedim+1,sizeof(char)); //+1 per il carattere terminatore
    if(read_file==NULL){
        perror("Calloc buffer lettura file da inviare");
        fclose(to_send);
        return -1;
    }
    //Leggo il file
    int n=(int)fread(read_file,sizeof(char),filedim,to_send);
    read_file[filedim]='\0';
    printf("Ho letto %d bytes\n",n);
    //printf("Ho letto %d bytes\nFILE LETTO:\n%s",n,read_file);
    //Chiudo il file
    if(fclose(to_send)==-1){
        perror("Errore fclose file da inviare");
        return -1;
    }
    //Invio al server il codice comando
    int op=6; filedim++;
    SYSCALL(ctrl,write(fd_connection,&op,sizeof(int)),"Errore nella 'write' del codice comando");

    //Invio pathname
    int pathdim=strlen(pathname)+1;
    SYSCALL(ctrl,write(fd_connection,&pathdim,sizeof(int)),"Errore nell'invio della dimensione del pathname");
    SYSCALL(ctrl,write(fd_connection,pathname,pathdim),"Errore nell'invio del pathname al server");

    //Invio al server il file letto
    SYSCALL(ctrl,write(fd_connection,&filedim,sizeof(int)),"Errore nella 'write' della dimensione (WriteFile)");
    SYSCALL(ctrl,write(fd_connection,read_file,filedim),"Errore nella 'write' della dimensione");
    printf("Ho scritto %d bytes\n",ctrl);
    free(read_file);

    //Ciclo per l'acquisizione degli eventuali file espulsi
    if(dirname){ //Solo se l'utente ha specificato una directory per la memorizzazione dei file espulsi
        int flag=1;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'write' dell flag di restituzione file");
        printf("Ho inviato il flag 1 per dirgli che voglio avere i file espulsi\n");

        //Attendo l'autorizzazione
        int authorized=-1;
        SYSCALL(ctrl,read(fd_connection,&authorized,sizeof(int)),"Errore nella ricezione del bit di autorizzazione");
        printf("Ho ricevuto il bit di autorizzazione %d\n",authorized);

        if(!authorized){
            printf("Non sei autorizzato alla scrittura del file %s!\n",pathname);
            return -1;
        }

        //Verifico che dirname sia effettivamente una cartella
        DIR* destination=opendir(dirname);
        if(!destination){
            if(errno=ENOENT){ //Se la cartella non esiste la creo
                mkdir(dirname,0777);
                destination=opendir(dirname);
                printf("Cartella per i file espulsi non trovata, la creo\n");
                if(!destination){
                    perror("Errore nell'apertura della cartella di destinazione dei file espulsi");
                    return -1;
                }
            }
        }
        printf("Ho aperto la cartella\n");
        int expelled_files_to_send;
        do{
            SYSCALL(ctrl,read(fd_connection,&expelled_files_to_send,sizeof(int)),"Errore nella 'read' del flag expelled_file");
            printf("C'e' qualche file da ricevere dal server?%d\n",expelled_files_to_send);

            if(expelled_files_to_send){ //C'e' qualche file da ricevere
                int dim_name,dim_content;

                //-----Lettura del nome del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_name,sizeof(int)),"Errore nella 'read' della dimensione del nome del file espulso");
                char* buffer_name=(char*)calloc(dim_name,sizeof(char));
                if(!buffer_name){
                    perror("Calloc");
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_name,dim_name),"Errore nella 'read' del nome del file espulso");
                printf("Devo ricevere il file con nome %s dim %d\n",buffer_name,dim_name);

                //-----Lettura del file espulso
                SYSCALL(ctrl,read(fd_connection,&dim_content,sizeof(int)),"Errore nella 'read' della dimensione del file espulso");
                char* buffer_content=(char*)calloc(dim_content,sizeof(char));
                if(!buffer_content){
                    free(buffer_name);
                    perror("Calloc");
                    return -1;
                }
                SYSCALL(ctrl,read(fd_connection,buffer_content,dim_content),"Errore nella 'read' del file espulso");
                printf("Ho ricevuto il contenuto %s di dim %d\n",buffer_content,dim_content);

                //Vado nella cartella destinazione
                chdir(dirname);
                char cwd[128];
                getcwd(cwd,sizeof(cwd));
                printf("Sono nella cartella %s\n",cwd);

                //Scrivo il nuovo file
                FILE* expelled_file=fopen(buffer_name,"w");
                if(!expelled_file){
                    free(buffer_name);
                    free(buffer_content);
                    return -1;
                }
                printf("Ho aperto il file!");
                free(buffer_name);
                fwrite(buffer_content,sizeof(char),dim_content-1,expelled_file);
                printf("Ho scritto il file\n");
                free(buffer_content);
                if(fclose(expelled_file)==-1){
                    perror("Errore nella chiusura del file");
                    return -1;
                }

                chdir("..");
                memset(cwd,'\0',128);
                getcwd(cwd,sizeof(cwd));
                printf("Sono nella cartella %s\n",cwd);
            }   
        }while(expelled_files_to_send);

        

        if(closedir(destination)==-1){
            perror("Errore in chiusura della cartella di destinazione dei file espulsi");
            return -1;
        }
    }else{
        int flag=0;
        SYSCALL(ctrl,write(fd_connection,&flag,sizeof(int)),"Errore nella 'read' dell flag di restituzione file");
    }
   
    //Leggo dal server la dimensione della risposta
    int dim=256;
    printf("ASPETTO UNA RISPOSTA\n");
    SYSCALL(ctrl,read(fd_connection,&dim,sizeof(int)),"Errore nella 'read' della dimensione della risposta");
    printf("Dimensione della risposta: %d\n",dim);

    //Alloco un buffer per leggere la risposta del server
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL)
        return EXIT_FAILURE;
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("Ho ricevuto dal server: %s\n",response);

    free(response);
    return 0;
}