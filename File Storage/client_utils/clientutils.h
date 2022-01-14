// created by Simone Schiavone at 20211128 16:18.
// @Università di Pisa
// Matricola 582418
#include <sys/stat.h>
#include <dirent.h>

void Welcome();
void PrintAcceptedOptions();
int Count_Commas(char* str);

typedef struct operation{
    char option; //codice dell'operazione
    int argc;
    char** args; //argomenti dell'operazione
}pending_operation;

typedef struct Node{ 
    pending_operation* op;
    struct Node *next; 
}operation_node; 

int list_insert_operation_end(operation_node** head,operation_node* to_insert);
void print_command_list(operation_node* head);
void list_destroy(operation_node* head);
void Execute_Requests(operation_node* head);


/*Lista di nomi di files*/
typedef struct file_name{
    struct file_name* next;
    char* name;
}file_name;

void name_list_destroy(file_name* head);
void print_name_list(file_name* head);
int files_in_directory(file_name** head,char* d);
int n_files_in_directory(file_name** head,char* d,int num);