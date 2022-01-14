// created by Simone Schiavone at 20211009 11:36.
// @Università di Pisa
// Matricola 582418

#define IF_PRINT_ENABLED(print) if(print_options){print}
#define OCREATE 1
#define OLOCK 2

int fd_connection;
char* backup_dir; //nome della cartella per la memorizzazione dei file espulsi
char* read_dir; //nome della cartella per la memorizzazione dei file letti
int print_options; //flag stampa
char working_directory[128]; //cartella di lavoro corrente

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* pathname,int flags);
int readFile(const char* pathname,void** buf,size_t* size);
int writeFile(char* pathname,char* dirname);
int lockFile(char* pathname);
int unlockFile(char* pathname);
int removeFile(char* pathname);
int appendToFile(char* pathname,void* buf,size_t size,char* dirname);
int readNFiles(int n, char* read_dir);
int closeFile(char* pathname);