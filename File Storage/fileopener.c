#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#define SYSCALL(r,c,e) if((r=c)==-1) {perror(e); return(-1);}

int writeFile(char* pathname,char* dirname){
    if(pathname==NULL){
        errno=EINVAL;
        perror("Parametri nulli");
        return -1;
    }
    FILE* to_send;
    if((to_send=fopen(pathname,"r"))==NULL){
        perror("Errore nell'apertura del file di log");
        return -1;
    }
    int size,ctrl;
    struct stat st;
    SYSCALL(ctrl,stat(pathname, &st),"Errore nella 'stat'");
    size = st.st_size;
    if(size==0){
        printf("FILE VUOTO\n");
    }
    unlink(pathname);
    return 0;
}
int main(){
	writeFile("Giovanni.txt",NULL);
	return 0;
}
