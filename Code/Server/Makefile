all:gestionVMS_MAIN

gestionVMS_MAIN: gestionVMS_MAIN.o gestionListeChaineeVMS.o gestionVMS.o
	gcc -o gestionVMS_MAIN gestionVMS_MAIN.o gestionListeChaineeVMS.o gestionVMS.o -lpthread
gestionVMS_MAIN.o: gestionVMS_MAIN.c gestionListeChaineeVMS.h gestionVMS.h
	gcc -c gestionVMS_MAIN.c -Wall -I.
gestionListeChaineeVMS.o: gestionListeChaineeVMS.c gestionListeChaineeVMS.h gestionVMS.h
	gcc -c gestionListeChaineeVMS.c -Wall -I.
gestionVMS.o: gestionVMS.c gestionListeChaineeVMS.h gestionVMS.h
	gcc -c gestionVMS.c -Wall -I.
clean:
	rm *.o

run:all
