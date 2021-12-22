echo "avvio server"
valgrind --leak-check=full ./server &
server_pid=$!
echo $server_pid
sleep 2
echo "avvio un client"
valgrind --leak-check=full ./client -p
sleep 2
echo "Client terminato invio segnale al server"
kill -s SIGINT $server_pid
echo "Segnale inviato"
sleep 2

