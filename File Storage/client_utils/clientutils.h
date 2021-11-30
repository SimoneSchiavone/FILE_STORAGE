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

int list_push(operation_node* head,operation_node* to_insert);
void list_destroy(operation_node* head);