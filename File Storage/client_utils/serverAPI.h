// created by Simone Schiavone at 20211009 11:36.
// @Università di Pisa
// Matricola 582418

int fd_connection;
char* backup_dir;
char* read_dir;
int w_or_W_to_do;

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(char* pathname,int o_create,int o_lock);
int readFile(char* pathname,void** buf,size_t* size);
int writeFile(char* pathname,char* dirname);
int lockFile(char* pathname);
int unlockFile(char* pathname);
int removeFile(char* pathname);
int appendToFile(char* pathname,void* buf,size_t size,char* dirname);