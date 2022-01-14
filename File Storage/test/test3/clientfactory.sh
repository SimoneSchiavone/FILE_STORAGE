while true
do
    ./client -t 0 -W ./test/test3/files/letteratura/divinacommedia.txt -R 1 -r ./test/test3/files/Mickey/randomtext.txt &
    ./client -t 0 -d Letti -r ./test/test3/files/Mickey/minnie.txt -r ./test/test3/files/filevuoto.txt -W ./test/test3/files/small/ansa.txt &
    ./client -t 0 -d Letti -R 2 -l ./test/test3/files/news/privacy.txt -u ./test/test3/files/news/privacy.txt &
    ./client -t 0 -l ./test/test3/files/news/clima.txt -u ./test/test3/files/news/clima.txt -R 1 &
    ./client -t 0 -d Letti -r ./test/test3/files/filevuoto.txt -r ./test/test3/files/small/topolino.txt -R 2&
    ./client -t 0 -l ./test/test3/files/Mickey/randomtext.pdf -c ./test/test3/files/Mickey/randomtext.pdf -W /test/test3/files/Mickey/topolino.txt &
    ./client -t 0 -d Letti -r ./test/test3/files/so.txt  -W ./test/test3/files/small/minnie.txt -R 3 &
    ./client -t 0 -d Letti -r ./test/test3/files/filevuoto.txt -r ./test/test3/files/news/ansa.txt -R 1 &
    ./client -t 0 -r ./test/test3/files/filevuoto.txt -R 2 -c ./test/test3/files/letteratura/montale.txt &
    ./client -t 0 -d Letti -r ./test/test3/files/filevuoto.txt -c ./test/test3/files/bigfile.txt -R 2&
    sleep 0.25
done

    #./client -t 0 -l ./test/test3/files/news/clima.txt -u ./test/test3/files/news/clima.txt &
    #./client -t 0 -d Letti -r ./test/test3/files/filevuoto.txt &
    #./client -t 0 -W ./test/test3/files/letteratura/divinacommedia.txt &
    #./client -t 0 -l ./test/test3/files/Mickey/randomtext.pdf -c ./test/test3/files/Mickey/randomtext.pdf &
    #./client -t 0 -d Letti -R 2 &
    #./client -t 0 -l ./test/test3/files/news/clima.txt -u ./test/test3/files/news/clima.txt &
    #./client -t 0 -d Letti -r ./test/test3/files/filevuoto.txt &
    #./client -t 0 -W ./test/test3/files/letteratura/divinacommedia.txt &
    #./client -t 0 -l ./test/test3/files/Mickey/randomtext.pdf -c ./test/test3/files/Mickey/randomtext.pdf &
    #./client -t 0 -d Letti -R 2 &