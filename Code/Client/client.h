#pragma once
#include <ncurses.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

WINDOW* setupTerminalWindow(bool boxed, bool writable, char title[], int beginX);
void* readInput(void* args);
void* readOutput(void* args);

struct window_socket {
    int fd;
    struct sockaddr_in address;
    WINDOW* win;
};

struct Info_FIFO_Transaction {
    int pid_client;
    char transaction[200];
};

