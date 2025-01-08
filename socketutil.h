#ifndef SOCKETUTIL_H
#define SOCKETUTIL_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "packet.h"

int create_tcp_ipv4_socket();
struct sockaddr_in* create_ipv4_address(char* ip, int port);

//=================== Server Side ==============================

typedef struct
{
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    bool acceptedSuccessfully;
    bool isAuth;
    char username[50];
    char password[50];
} client_connection;

void bind_socket(int serverSocketFD, struct sockaddr_in* address);
void listen_for_connections(int serverSocketFD);

// void start_accepting_connections(int serverSocketFD);
client_connection* accept_incoming_connection(int serverSocketFD);

//==============================================================

//=================== Client Side ==============================

void connect_to_server(int serverSocketFD, struct sockaddr_in* address);
void connect_to_server_auth(int socketFD, struct sockaddr_in *address, char *username, char *password);

#endif
