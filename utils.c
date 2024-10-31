#include "utils.h"

void set_log_file(const char *filename, int level)
{
    FILE *log_file = fopen(filename, "a");

    if (log_file == NULL)
    {
        perror("[set_log_file(const char*)] ");
        return;
    }

    log_add_fp(log_file, level);
}


void send_data_from_terminal(int socketFD)
{
    char *line = NULL;
    size_t lineSize = 0;

    while (true)
    {
        ssize_t rc = getline(&line, &lineSize, stdin);
        line[rc - 1] = 0;

        if (rc > 0)
        {
            if (strncmp(line, "exit", 4) == 0)
                break;

            send(socketFD, line, strlen(line), 0);
        }
    }
}

