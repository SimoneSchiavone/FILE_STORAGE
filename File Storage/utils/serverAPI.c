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
    return EXIT_SUCCESS;
}