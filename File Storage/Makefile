CC = gcc
CFLAGS = -Wall -pedantic-errors -g -std=gnu99
PTHREADS = -lpthread
.PHONY = all clear test1 test2 test3 result_clean

client_modules = client_utils
server_modules = server_utils
txt_files_dir = txt
test_dir = test
server_dependecies = $(server_modules)/data_structures.o $(server_modules)/serverutils.o server.c
client_dependecies = $(client_modules)/clientutils.c $(client_modules)/FileStorageAPILib.a client.c


all: server client

server: $(server_dependecies) 
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREADS)

client: $(client_dependecies)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREADS) -L ./$(client_modules) $(client_modules)/FileStorageAPILib.a

$(client_modules)/FileStorageAPILib.a: $(client_modules)/serverAPI.o
	ar rvs $@ $<

$(server_modules)/%.o: $(server_modules)/%.c $(server_modules)/%.h
	$(CC) -c $(CFLAGS) $< -o $@ $(PTHREADS)

$(client_modules)/%.o: $(client_modules)/%.c $(client_modules)/%.h
	$(CC) -c $(CFLAGS)	 $< -o $@ $(PTHREADS)

clean:
	-rm -f -v server client *.sock *~ $(server_modules)/*.o $(client_modules)/*.o $(client_modules)/*.a Espulsi/* Letti/* $(txt_files_dir)/journal.txt
	-rmdir Espulsi Letti

test1:
	make result_clean
	@clear
	@echo "----------INIZIO TEST #1----------"
	cp $(test_dir)/test1/test1.txt $(txt_files_dir)/config_file.txt
	@bash $(test_dir)/test1/test1.sh
	@echo "----------FINE TEST #1----------"
	@./statistiche.sh

test2:
	make result_clean
	@clear
	@echo "----------INIZIO TEST #2----------"
	cp $(test_dir)/test2/test2.txt $(txt_files_dir)/config_file.txt
	@bash $(test_dir)/test2/test2.sh
	@echo "----------FINE TEST #2----------"
	@./statistiche.sh

test3:
	make result_clean
	@clear
	@echo "----------INIZIO TEST #3----------"
	cp $(test_dir)/test3/test3.txt $(txt_files_dir)/config_file.txt
	@bash $(test_dir)/test3/test3.sh
	@sleep 1
	@echo "----------FINE TEST #3----------"
	@./statistiche.sh
	
result_clean:
	@echo "----------RESULT CLEAN----------"
	-rm -f -v Espulsi/* Letti/* $(txt_files_dir)/journal.txt
	-rmdir Espulsi Letti
	@echo "----------FINE RESULT CLEAN----------"

