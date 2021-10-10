// created by Simone Schiavone at 20211009 11:36.
// @Università di Pisa
// Matricola 582418

int fd_connection;

int openConnection(const char* sockname, int msec, const struct timespec abstime);
int closeConnection(const char* sockname);