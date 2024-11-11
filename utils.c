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

void send_packet(int socketfd, packet *packet)
{
    if (!packet)
    {
        log_debug("[send_packet(int, packet*)] : NULL packet pointer");
        return;
    }

    ssize_t bytes_sent = send(socketfd, packet, sizeof(packet_type) + strlen(packet->payload), 0);

    if (bytes_sent == -1)
    {
        log_error("[send_packet(int, packet*)] : &s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (bytes_sent < sizeof(packet_type) + strlen(packet->payload))
    {
        log_warn("[send_packet(int, packet*)] : incomplete packet sent (%zd of %zu bytes)", bytes_sent, sizeof(packet_type) + strlen(packet->payload));
    }
    else if (bytes_sent == 0)
    {
        log_info("[send_packet(int, packet*)] : Connection closed by server");
    }
}