echo "avvio server"
valgrind --leak-check=full ./server &
server_pid=$!
echo $server_pid
sleep 2
echo "avvio un client"
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 200 -W File_di_prova/minnie.txt -D Espulsi -d Letti -r File_di_prova/minnie.txt -u File_di_prova/minnie.txt 

valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 200 -l File_di_prova/minnie.txt -W File_di_prova/minnie.txt,File_di_prova/ansa.txt,File_di_prova/greepass.pdf -d Letti -c File_di_prova/minnie.txt -R

sleep 2
echo "Client terminato invio segnale al server"
kill -s SIGINT $server_pid
echo "Segnale inviato"
sleep 2

