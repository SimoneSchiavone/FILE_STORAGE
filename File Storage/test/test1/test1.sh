echo "avvio server"
valgrind --leak-check=full ./server &
server_pid=$!
echo $server_pid
sleep 2
echo "avvio un client"
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 1000 -W File_di_prova/minnie.txt -D Espulsi -d Letti -r File_di_prova/minnie.txt -c File_di_prova/minnie.txt 
#valgrind --leak-check=full ./client -p
sleep 2
echo "Client terminato invio segnale al server"
kill -s SIGINT $server_pid
echo "Segnale inviato"
sleep 2

