#!/bin/bash
clear
chmod +x client.sh
gcc -Wall -Werror -pedantic-errors -g -c client_utils/serverAPI.c -o client_utils/serverAPI.o
ar rvs client_utils/FileStorageAPILib.a client_utils/serverAPI.o
gcc -Wall -Werror -pedantic-errors -g client_utils/clientutils.c client.c -o client -lpthread -L ./client_utils client_utils/FileStorageAPILib.a
#valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -f hola -w SanMartino,5 -W c,i,a -D gg -r g,h,j -d sd -t 6 -l as,df,gh -u as,df,gh -c as,df,gh -p#
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 1000 -W File_di_prova/minnie.txt -D Espulsi -d Letti -r File_di_prova/minnie.txt -c File_di_prova/minnie.txt 
