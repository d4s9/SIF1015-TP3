all:main

main: main.o client.o
	gcc -o main main.o client.o -lncurses -lpthread
main.o: main.c client.h
	gcc -c main.c -Wall -I.
client.o: client.c client.h
	gcc -c client.c -Wall -I.
clean:
	rm *.o

run:all