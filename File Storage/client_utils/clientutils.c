// created by Simone Schiavone at 20211128 16:17.
// @Università di Pisa
// Matricola 582418
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "clientutils.h"

void Welcome(){
    //system("clear");
    printf(" ______ _____ _      ______    _____ _______ ____  _____            _____ ______ \n");
    printf("|  ____|_   _| |    |  ____|  / ____|__   __/ __ \\|  __ \\     /\\   / ____|  ____|\n");
    printf("| |__    | | | |    | |__    | (___    | | | |  | | |__) |   /  \\ | |  __| |__ \n");
    printf("|  __|   | | | |    |  __|    \\___ \\   | | | |  | |  _  /   / /\\ \\| | |_ |  __|\n");
    printf("| |     _| |_| |____| |____   ____) |  | | | |__| | | \\ \\  / ____ \\ |__| | |____\n");
    printf("|_|    |_____|______|______| |_____/   |_|  \\____/|_|  \\_\\/_/    \\_\\_____|______|\n");
    printf("***Client File Storage ATTIVO***\n\n");
}

void PrintAcceptedOptions(){
    printf("*****Opzioni ACCETTATE*****\n");
    printf("\t '-f filename' \n\tspecifica il nome del socket AF_UNIX a cui connettersi\n\n");
    printf("\t '-w dirname[,n=0]'\n\t invia al server i file nella cartella ‘dirname’ Se la directory ‘dirname’ contiene\n\t altre directory, queste vengono visitate ricorsivamente fino a quando non si leggono ‘n‘ \n\t file; se n=0 (o non è specificato) non c’è un limite superiore al numero di file da \n\t inviare al server (tuttavia non è detto che il server possa scriverli tutti).\n\n");
    printf("\t '-W file1[,fil2]' \n\t lista di nomi di file da scrivere nel server separati da ','\n\n");
    printf("\t '-D dirname' \n\t specifica la cartella in memoria secondaria dove vengono memorizzati i file che il server\n\t rimuove a seguito di capacity misses ottenuti a causa di scritture richieste attraverso le \n\t opzioni -W e -w. L'opzione -D deve essere usata congiuntamente all'opzione -w o -W \n\t altrimenti è generato un errore. Se l'opzione -D non viene specificata tutti \n\t i file che il server invia verso il client in seguito a capacity misses vengono persi. \n\n");
    printf("\t '-r file1[,file2]' \n\t lista di nomi di file da leggere dal server separati da ','\n\n");
    printf("\t '-R [n=0]' \n\t tale opzione permette di leggere n file qualsiasi attualmente memorizzati nel server. se \n\t n=0 vengono letti tutti i file presenti nel server.\n\n");
    printf("\t '-d dirname' \n\t cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione -r o -R.\n\t L'opzione -d va usata congiuntamente a '-r' o '-R' altrimenti viene generato un errore; Se\n\t vengono utilizzate le opzioni '-r' o '-R' senza specificare l'opzione '-d' i file \n\t letti non vengono memorizzati sul disco\n\n");
    printf("\t '-t time' \n\t tempo in millisecondi che intercorre tra l'invio di due richieste successive al server. \n\t Se time=0 non vi è alcun ritardo tra l'invio di due richieste consecutive.\n\n");
    printf("\t '-l file1[,file2]' \n\t lista di file su cui acquisire la mutua esclusione\n\n");
    printf("\t '-u file1[,file2]' \n\t lista di fule su cui rilasciare la mutua esclusione\n\n");
    printf("\t '-c file1[,file2]' \n\t lista di file da rimuovere nel server se presenti\n\n");
    printf("\t '-p' \n\t abilita le stampe sullo standard output per ogni operazione\n");
    printf("*****************\n");
}

int Count_Commas(char* str){
    //printf("Optarg %s\n",str);
    int c=0;
    for(int i=0;i<strlen(str);i++){
        //printf("Analizzo %c\n",str[i]);
        if(str[i]==','){
            //printf("Trovata una virgola\n");
            c++;
        }
    }
    return c;
}

void print_command_list(operation_node* head){
    printf("----------\n");
    printf("Operation List\n");
    operation_node* curr=head;
    int i=0;
    while (curr){
        printf("Comando nr %d\n\tOperazione %c\n\tArgc %d\n\tArgomenti:\n\t\t",i++,curr->op->option,curr->op->argc);
        int w;
        for(w=0;w<curr->op->argc;w++){
            printf("%s\n\t\t",curr->op->args[w]);
        }
        if(w==0)
            printf(" (None)\n");
        printf("\n");
        curr=curr->next;
    }
}

int list_insert_operation_end(operation_node** head,operation_node* to_insert){
    if(!to_insert)    
        return -1;
    if(*head==NULL){
        *head=to_insert;
        return EXIT_SUCCESS;
    }
    operation_node* curr=*head;
    while(curr->next!=NULL)
        curr=curr->next;
    to_insert->next=NULL;
    curr->next=to_insert;
    return EXIT_SUCCESS;
}

int list_insert_operation_start(operation_node** head,operation_node* to_insert){
    if(!to_insert)    
        return -1;
    to_insert->next=*head;
    *head=to_insert;
    return EXIT_SUCCESS;
}

void list_destroy(operation_node* head){
    operation_node* curr=head;
    operation_node* tmp;
    while(curr != NULL){
        tmp=curr->next;
        for(int i=0;i<curr->op->argc;i++){
            if(curr->op->args[i])
                free(curr->op->args[i]);
        }
        //le stringhe sono allocate sullo stack
        if(curr->op->args)
            free(curr->op->args);
        free(curr->op);
        free(curr);
        curr=tmp;
    }
}

void print_name_list(file_name* head){
    printf("---Name List---\n");
    file_name* curr=head;
    while (curr){
        printf("%s -> ",curr->name);
        curr=curr->next;
    }
    printf("NULL\n");
}

int list_insert_name(file_name** head,char* name){
    if(!name)    
        return -1;
    file_name* to_insert=(file_name*)malloc(sizeof(file_name));
    if(!to_insert)
        return -1;
    to_insert->next=(*head);
    to_insert->name=strdup(name);
    (*head)=to_insert;
    return EXIT_SUCCESS;
}

int is_file_name_in_list(file_name* head,char* name){
    printf("Nome da cercare %s\n",name);
    file_name* curr=head;
    while(curr){
        printf("Confronto con %s\n",curr->name);
        if(strcmp(curr->name,name)==0)
            return 1;
        curr=curr->next;
    }
    return 0;
}

void name_list_destroy(file_name* head){
    file_name* tmp=head;
    while(head){
        tmp=head;
        head=head->next;
        free(tmp->name);
        free(tmp);
    }
}

int isdot(const char dir[]) {
    int l = strlen(dir);
    if ( (l>0 && dir[l-1] == '.') ) return 1;
    return 0;
}

int files_in_directory(file_name** head,char* dirname){
    if(chdir(dirname)==-1){
        fprintf(stderr,"Errore cambiando la cartella\n");
        return -1;
    }

    //Apertura cartella
    DIR* radix=opendir(".");
    if(!radix){
        fprintf(stderr,"Errore nell'apertura della cartella corrente\n");
        return -1;
    }
    struct dirent* currentfile=NULL;
    currentfile=readdir(radix);
    while(currentfile){
        struct stat c;
        if(stat(currentfile->d_name,&c)==-1){
            fprintf(stderr,"Errore nella stat\n");
            closedir(radix);
            return -1;
        }
        if(S_ISDIR(c.st_mode)){ //Se e' una directory ricorro
            if(!isdot(currentfile->d_name)){
		        printf("%s e' una cartella, ricorro\n",currentfile->d_name);
	            files_in_directory(head,currentfile->d_name);
	            chdir("..");
	        }
        }else{ //Se e' un file inserisco il suo nome in lista
            file_name* to_insert=(file_name*)malloc(sizeof(file_name));
            to_insert->name=strdup(currentfile->d_name);
            to_insert->next=(*head);
            (*head)=to_insert;
        }
        currentfile=readdir(radix);
    }
    if(closedir(radix)==-1){
        fprintf(stderr,"Errore nella chiusura della cartella %s\n",dirname);
        return -1;
    }
    return 0;
}

int n_files_in_directory(file_name** head,char* dirname,int num){
    if(chdir(dirname)==-1){
        fprintf(stderr,"Errore cambiando la cartella\n");
        return -1;
    }

    //Apertura cartella
    DIR* radix=opendir(".");
    if(!radix){
        fprintf(stderr,"Errore nell'apertura della cartella corrente\n");
        return -1;
    }
    struct dirent* currentfile=NULL;
    currentfile=readdir(radix);
    while(currentfile){
        if(!num){
            closedir(radix);
            return 0;
        }

        struct stat c;
        if(stat(currentfile->d_name,&c)==-1){
            fprintf(stderr,"Errore nella stat\n");
            closedir(radix);
            return -1;
        }
        if(S_ISDIR(c.st_mode)){ //Se e' una directory ricorro
            if(!isdot(currentfile->d_name)){
		        printf("%s e' una cartella, ricorro\n",currentfile->d_name);
	            n_files_in_directory(head,currentfile->d_name,num);
	            chdir("..");
	        }
        }else{ //Se e' un file inserisco il suo nome in lista
            file_name* to_insert=(file_name*)malloc(sizeof(file_name));
            to_insert->name=strdup(currentfile->d_name);
            to_insert->next=(*head);
            (*head)=to_insert;
        }
        currentfile=readdir(radix);
        num--;
    }
    if(closedir(radix)==-1){
        fprintf(stderr,"Errore nella chiusura della cartella %s\n",dirname);
        return -1;
    }
    return 0;
}

