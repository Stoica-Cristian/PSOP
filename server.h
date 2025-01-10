#ifndef SERVER_h
#define SERVER_h

#include "exchange.h"

#define MAX_PROCESSED_IDS 1000

direct_exchange *direct_exch;
topic_exchange *topic_exch;

user **users;

typedef struct client_connection
{
    char username[50];
    char password[50];
    int acceptedSocketFD;
    bool isAuth;
    struct sockaddr_in address;
    bool acceptedSuccessfully;
    int error;
} client_connection;

void bind_socket(int serverSocketFD, struct sockaddr_in* address);
void listen_for_connections(int serverSocketFD);

client_connection* accept_incoming_connection(int serverSocketFD);

void receive_incoming_data(client_connection *established_connection);
void *process_packets(void *arg);
cJSON *parse_packet_payload_to_json(packet *packet);

void handle_producer_publish(int socketFD, packet *packet_received);
void handle_consumer_request(int socketFD, packet *packet_received);
void handle_subscribe(int socketFD, packet *packet_received, client_connection *established_connection);
void handle_user_authentication(int socketFD, packet *packet_received, client_connection *established_connection);
void handle_disconnect(int socketFD, client_connection *established_connection);

int create_tcp_ipv4_socket();
struct sockaddr_in* create_ipv4_address(char* ip, int port);

void send_bad_format_packet(int socketFD, const unique_id *uuid);
void send_producer_ack_packet(int socketFD, const unique_id *uuid);
void send_incomplete_packet(int socketFD, const unique_id *uuid);
void send_queue_not_found_packet(int socketFD, const unique_id *uuid);

char processed_ids[MAX_PROCESSED_IDS][37]; // Stocăm până la 100 de ID-uri procesate
int processed_count = 0;

bool is_duplicate(const char *packet_id);
void store_packet_id(const char *packet_id);

#endif