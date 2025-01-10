#ifndef MESSAGE_BROKER_H
#define MESSAGE_BROKER_H

#include "packet.h"

typedef struct client_connection_info
{
    int socketFD;
    struct sockaddr_in *address;
} client_connection_info; 

extern bool ack_received;
extern unique_id current_id;
extern int ack_count;

void *process_packets(void *client_info);
void handle_consumer_response(packet packet);

// connection functions

int create_tcp_ipv4_socket();
struct sockaddr_in* create_ipv4_address(char* ip, int port);

void connect_to_server();
void connect_to_server_auth(char *username, char *password);

// packet functions

void client_send_packet(packet *packet);
void send_request_packet(const char *type, const char *identifier);
void send_subscribe_packet(const char *topic);
void send_disconnect_packet();
void send_messages_from_json_file(const char *filename);

char **read_and_parse_json_from_file(const char *filename, int *message_count);

#endif
