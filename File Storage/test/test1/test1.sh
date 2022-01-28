echo "Avvio server"
valgrind --leak-check=full ./server &
server_pid=$!
echo $server_pid

sleep 2

valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./client -p -t 200 -w ./test/test1/File_di_prova -D Espulsi -r ./test/test1/File_di_prova/minnie.txt -u ./test/test1/File_di_prova/minnie.txt 
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./client -p -t 200 -l ./test/test1/File_di_prova/minnie.txt -W ./test/test1/File_di_prova/minnie.txt -d Letti -R 0 -c ./test/test1/File_di_prova/minnie.txt

sleep 2
echo "Client terminato invio segnale al server"
kill -s SIGHUP $server_pid
echo "Segnale inviato"
sleep 2

