#avvio il server
./server &
server_pid=$!
echo "->Avvio Server $server_pid"

sleep 2

valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./client -t 0 -w ./test/test3/files

chmod +x test/test3/clientfactory.sh
./test/test3/clientfactory.sh &
script_pid=$!

echo "-> Avvio script con pid $script_pid"
sleep 30
kill -s SIGINT $server_pid
kill $script_pid

echo "Segnale inviato"
