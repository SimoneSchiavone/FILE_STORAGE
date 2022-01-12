./server &
server_pid=$!
echo "->Avvio Server $server_pid"

sleep 2

valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -t 0 -w ./test/test3/files
for i in $(seq 1 1 30)
do
    echo "Welcome $i times"
    ./client -t 0 -p -l ./test/test3/files/news/clima.txt -u ./test/test3/files/news/clima.txt &
    ./client -t 0 -p -d Letti -r ./test/test3/files/filevuoto.txt &
    ./client -t 0 -p -W ./test/test3/files/letteratura/divinacommedia.txt &
    ./client -t 0 -p -l ./test/test3/files/Mickey/randomtext.pdf -c ./test/test3/files/Mickey/randomtext.pdf
    ./client -t 0 -p -d Letti -R 2
    ./client -t 0 -p -l ./test/test3/files/news/clima.txt -u ./test/test3/files/news/clima.txt &
    ./client -t 0 -p -d Letti -r ./test/test3/files/filevuoto.txt &
    ./client -t 0 -p -W ./test/test3/files/letteratura/divinacommedia.txt &
    ./client -t 0 -p -l ./test/test3/files/Mickey/randomtext.pdf -c ./test/test3/files/Mickey/randomtext.pdf
    ./client -t 0 -p -d Letti -R 2
done

sleep 15
kill -s SIGINT $server_pid
echo "Segnale inviato"
