#ifndef UTILS_H
#define UTILS_H

#include "packet.h"

void set_log_file(const char *filename,  int level);

void send_data_from_terminal(int socketFD);

#endif