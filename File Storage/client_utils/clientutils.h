// created by Simone Schiavone at 20211128 16:18.
// @Università di Pisa
// Matricola 582418

void Welcome();
void PrintAcceptedOptions();
int Count_Commas(char* str);

typedef struct operation{
    int op_code; //codice dell'operazione
    int argc;
    char** args; //argomenti dell'operazione
}pending_operation;

typedef struct Node{ 
    pending_operation* op;
    struct Node *next; 
}operation_node; 

int list_insert_end(operation_node** head,operation_node* to_insert);
int list_insert_start(operation_node** head,operation_node* to_insert);
void print_command_list(operation_node* head);
void list_destroy(operation_node* head);