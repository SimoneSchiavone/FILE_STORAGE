clear
chmod +x server.sh
#!/bin/bash
gcc -Wall -Werror -pedantic-errors -g server_utils/serverutils.c server_utils/data_structures.c server.c -o server -lpthread
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./server
