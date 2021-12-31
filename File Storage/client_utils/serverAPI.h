// created by Simone Schiavone at 20211009 11:36.
// @Università di Pisa
// Matricola 582418

#define IF_PRINT_ENABLED(print) if(print_options){print}
#define OCREATE 1
#define OLOCK 2

int fd_connection;
char* backup_dir;
char* read_dir;
int print_options;
char working_directory[128];

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
//int openFile(const char* pathname,int o_create,int o_lock);
int openFile(const char* pathname,int flags);
int readFile(const char* pathname,void** buf,size_t* size);
int writeFile(char* pathname,char* dirname);
int lockFile(char* pathname);
int unlockFile(char* pathname);
int removeFile(char* pathname);
int appendToFile(char* pathname,void* buf,size_t size,char* dirname);
int readNFiles(int n, char* read_dir);
int closeFile(char* pathname);