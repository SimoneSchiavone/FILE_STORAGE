#!/bin/bash
clear
chmod +x client.sh

#valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 1000 -W File_di_prova/minnie.txt -D Espulsi -d Letti -r File_di_prova/minnie.txt -c File_di_prova/minnie.txt 
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 1500 -w ../File_di_prova -D Espulsi -u ../File_di_prova/minnie.txt
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -p -t 1500 -l ../File_di_prova/minnie.txt -W ../File_di_prova/minnie.txt -d Letti -r ../File_di_prova/minnie.txt -c ../File_di_prova/minnie.txt -R