clear
chmod +x server.sh
gcc -Wall -Werror -pedantic-errors -g client_utils/clientutils.c client_utils/serverAPI.c client.c -o client -lpthread
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all -s ./client -f hola -w SanMartino,5 -W c,i,a -D gg -r g,h,j -d sd -t 6 -l as,df,gh -u as,df,gh -c as,df,gh -p
