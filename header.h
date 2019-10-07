#pragma once
#define _POSIX_C_SOURCE 199309L
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>      
#include <sys/types.h>  
#include <sys/stat.h>      
#include <arpa/inet.h>           
#include <netinet/in.h>
#include <netdb.h>      
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>


#define SOCKET_TIMEOUT  10

#define MAX_LINE        1000

#define PORT            25000

#define MAX_PENDING     5

#define REQUESTCODE1    1

#define REQUESTCODE2    2

#define Error_(x,y) { puts(x); exit(y); }

//function declarations.

ssize_t Readline(int socket_descriptor, void *vptr, size_t maxlenght);

ssize_t Writeline(int socket_descriptor, const void *vptr, size_t n);

int Lines_counter( FILE * f );

void display();

void try_connection();

int request_1();

int request_2();

int request_3();

int quit();

int Writefile( int socket_descriptor, FILE *f);



