#pragma once
#include <semaphore.h>
#include <pthread.h>

#include <stdint.h>
#include <sys/stat.h>
#include <signal.h>
/* unix */
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <errno.h>

struct infoVM{						
	int	noVM;
	unsigned char busy;
	pthread_t tid;
	int DimRam;
	int DimRamUsed;
	uint16_t* ptrDebutVM;
	uint16_t offsetDebutCode; // region memoire ReadOnly
	uint16_t offsetFinCode;
};

struct noeudVM{			
	struct infoVM VM;
	struct noeudVM *suivant;
    sem_t semN;
};

struct paramE {
	int noVM;
	int client_socket_fd;
};

struct paramReadTrans {
	int client_socket_fd;
};

struct paramX
{
	int boolPrint;
	char fileName[100];
	int client_socket_fd;
};

struct paramL{
	int nStart;
	int nEnd;
	int client_socket_fd;
};

struct paramCommand {
	char command[100];
	int client_socket_fd;
};

struct paramK {
	pthread_t tid;
};

struct Info_FIFO_Transaction {
	int client_pid;
	char transaction[200];
};

struct client_FIFO_Info {
	char path[100];
};
	
void cls(void);
void error(const int exitcode, const char * message);

struct noeudVM * findItem(const int no);
struct noeudVM * findFreeVM();
struct noeudVM * findPrev(const int no);

struct noeudVM * addItem();
//void* listOlcFile(void* args); // fichier binaire
void* removeItem(void* noVM);
void* listItems(void* args);
void saveItems(const char* sourcefname);
void* executeFile(void* args);
void* killThread(void* args);
void* sendCommand(void* args);

void* readTrans(void* args);
void* receiveTrans();
