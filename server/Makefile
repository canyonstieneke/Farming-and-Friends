compile:
	gcc -c `mysql_config --cflags` main.c
	gcc -o faf_banker_bot main.c `mysql_config --libs` -pthread -std=c99
