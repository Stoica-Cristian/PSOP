#ifndef PACKET_H
#define PACKEt_H

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
#include "cJSON.h"

// #define STRUCTURED_PACKET

#define MAX_PACKET_SIZE 8192

#ifdef STRUCTURED_PACKET
#define INITIAL_PAYLOAD_SIZE 64
#define PAYLOAD_INCREMENT 64
#endif

typedef enum packet_type
{
    PKT_UNKNOWN,
    PKT_BAD_FORMAT, // bad json format
    PKT_INCOMPLETE, // incomplete fields
    PKT_EMPTY_QUEUE, // after timer expires
    PKT_PRODUCER_PUBLISH,
    PKT_PRODUCER_ACK,
    PKT_PRODUCER_NACK,
    PKT_CONSUMER_REQUEST,
    PKT_CONSUMER_RESPONSE,
    PKT_CONSUMER_ACK,
    PKT_CONSUMER_NACK,
} packet_type;

typedef struct packet
{
    packet_type packet_type;
    char payload[MAX_PACKET_SIZE];

#ifdef STRUCTURED_PACKET
    int payload_size;
    int payload_capacity;
    int extraction_offset;
#endif
} packet;

packet *packet_create(packet_type type);
void packet_destroy(packet *packet);

void packet_set_payload(packet *packet, const char *data, int size);

void send_packet(int socketfd, packet *packet);
void send_bad_format_packet(int socketFD);
void send_producer_ack_packet(int socketFD);
void send_incomplete_packet(int socketFD);
void send_consumer_request_packet(int socketFD, const char* type, const char *identifier);

#ifdef STRUCTURED_PACKET
void packet_expand_payload(packet *packet, int additional_size);

void packet_append_int(packet *packet, int data);
void packet_append_string(packet *packet, const char *data);

int packet_extract_int(packet *packet);
char *packet_extract_string(packet *packet);

#endif

#endif