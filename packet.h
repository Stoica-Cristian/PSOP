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
#include <time.h>

#include "log.h"
#include "cJSON.h"

#ifdef __has_include
#if __has_include(<uuid/uuid.h>)
#include <uuid/uuid.h>
#define HAS_UUID 1
#else
#define HAS_UUID 0
#endif
#else
#define HAS_UUID 0
#endif

typedef struct unique_id
{
#if HAS_UUID
    uuid_t uuid; // tip uuid nativ
#else
    char custom_id[37]; // UUID "custom", format de 36 caractere + '\0'
#endif
} unique_id;

void generate_custom_uid(char *custom_id);
void generate_uid(unique_id *id);
void uid_to_string(const unique_id *id, char *str);
void print_uid(const unique_id *id);
void copy_uid(unique_id *dest, const unique_id *src);
bool uid_equals(const unique_id *uid1, const unique_id *uid2);

#define MAX_PACKET_SIZE 8192

typedef enum packet_type
{
    PKT_UNKNOWN,
    PKT_BAD_FORMAT,  // bad json format
    PKT_INCOMPLETE,  // incomplete fields
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
    unique_id id;
    char payload[MAX_PACKET_SIZE];
} packet;

packet *new_packet(packet_type type, const char *payload);
packet create_packet(packet_type type, const char *payload);
char* packet_type_to_string(packet_type type);
void print_packet(const packet *packet);
void packet_destroy(packet *packet);

void packet_set_payload(packet *packet, const char *data, int size);

void send_packet(int socketfd, packet *packet);
void send_bad_format_packet(int socketFD, const unique_id *uuid);
void send_producer_ack_packet(int socketFD, const unique_id *uuid);
void send_incomplete_packet(int socketFD, const unique_id *uuid);
void send_consumer_request_packet(int socketFD, const char *type, const char *identifier);

#endif