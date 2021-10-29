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
    if(response==NULL)
        return EXIT_FAILURE;
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("[OpenFile]Ho ricevuto dal server %s\n",response);
    /*
    if(strncmp(response,"OK",2)!=0)
        return EXIT_FAILURE;
        */
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
    if(!pathname || !buf || !size){
        errno=EINVAL;
        perror("Errore parametri nulli");
        return -1;
    }
    //TODO Mettere const char...
    int op=3,dim=strlen(pathname)+1,ctrl;
    //Invio il codice comando
    SYSCALL(ctrl,write(fd_connection,&op,4),"Errore nella 'write' del codice comando");
    //Invio la dimensione di pathname
    SYSCALL(ctrl,write(fd_connection,&dim,4),"Errore nella 'write' della dimensione del pathname");
    //Invio la stringa pathname
    SYSCALL(ctrl,write(fd_connection,pathname,dim),"Errore nella 'write' di pathname");

    //Attendo dimensione della risposta
    SYSCALL(ctrl,read(fd_connection,&dim,4),"Errore nella 'read' della dimensione della risposta");
    printf("Dimensione della risposta: %d\n",dim);
    char* response=(char*)calloc(dim+1,sizeof(char));
    if(response==NULL){
        perror("Malloc response");
        return -1;
    }
    SYSCALL(ctrl,read(fd_connection,response,dim),"Errore nella 'read' della risposta");
    printf("[OpenFile]Ho ricevuto dal server %s\n",response);
    free(response);

    //Attendo l'indirizzo (dimensione 8)
    SYSCALL(ctrl,read(fd_connection,buf,8),"Errore nella 'read' della risposta");
    printf("Indirizzo area di memoria %p\n",*buf);
    return 0;
}

int writeFile(char* pathname,char* dirname){
    if(pathname==NULL){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        perror("Errore nell'apertura da inviare al server");
        return -1;
    }
    int size,ctrl;
    struct stat st;
    SYSCALL(ctrl,stat(pathname, &st),"Errore nella 'stat'");
    size = st.st_size;
    int filedim=(int)size;

    //Allochiamo un buffer per la lettura del file
    char line[128];
    char* read_file=(char*)calloc(filedim,sizeof(char));
    if(read_file==NULL){
        perror("Callol buffer lettura file da inviare");
        return -1;
    }
    while(fgets(line,128,to_send)!=NULL){

    }
    return 0;
}