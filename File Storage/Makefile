CC = gcc
CFLAGS = -Wall -pedantic-errors -Werror -g
PTHREADS = -lpthread
.PHONY = all clear test1 pippo

client_modules = client_utils
server_modules = server_utils
server_dependecies = $(server_modules)/data_structures.o $(server_modules)/serverutils.o server.c
client_dependecies = $(client_modules)/clientutils.c $(client_modules)/FileStorageAPILib.a client.c

all: server client

server: $(server_dependecies) 
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREADS)

client: $(client_dependecies)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREADS) -L ./$(client_modules) $(client_modules)/FileStorageAPILib.a

$(client_modules)/FileStorageAPILib.a: $(client_modules)/serverAPI.o
	ar rvs $@ $<

#regola per compilare ogni modulo nel suo file oggetto
$(server_modules)/%.o: $(server_modules)/%.c $(server_modules)/%.h
	$(CC) -c $(CFLAGS) $< -o $@ $(PTHREADS)

$(client_modules)/%.o: $(client_modules)/%.c $(client_modules)/%.h
	$(CC) -c $(CFLAGS)	 $< -o $@ $(PTHREADS)

#rimuove tutti i file testuali tranne quello di configurazione
clean:
	-rm -f -v server client *.sock *~ $(server_modules)/*.o $(client_modules)/*.o $(client_modules)/*.a Espulsi/* Letti/*
	-rmdir Espulsi Letti
test1:
	@clear
	@echo "----------INIZIO TEST #1----------"
	cp test/test1/test1.txt txt/config_file.txt
	@bash test/test1/test1.sh
	@echo "----------FINE TEST #1----------"
	@./statistiche.sh

test2:
	@bash testsighup.sh