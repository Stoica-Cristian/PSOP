#ifndef UTILS_H
#define UTILS_H

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
#include <fcntl.h>

#include "log.h"
#include "packet.h"

void set_log_file(const char *filename,  int level);

void send_data_from_terminal(int socketFD);

void send_packet(int socketfd, packet *packet);

#endif