echo "avvio server"
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./server &
server_pid=$!
echo $server_pid

sleep 2

valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -t 4500 -w ./test/test2/files -D Espulsi 
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -D Espulsi -W test/test2/files/so.txt,test/test2/files/so.txt

sleep 2
echo "Client terminato invio segnale al server"
kill -s SIGHUP $server_pid
echo "Segnale inviato"
sleep 2
