#ifndef PACKET_H
#define PACKEt_H

#include "log.h"
#include "utils.h"

// #define STRUCTURED_PACKET

#define MAX_PACKET_SIZE 8192

#ifdef STRUCTURED_PACKET
#define INITIAL_PAYLOAD_SIZE 64
#define PAYLOAD_INCREMENT 64
#endif

typedef enum
{
    PKT_UNKNOWN,
    PKT_PRODUCER_PUBLISH,
    PKT_PRODUCER_ACK,
    PKT_PRODUCER_NACK,
    PKT_CONSUMER_REQUEST,
    PKT_CONSUMER_ACK,
    PKT_CONSUMER_NACK,
} packet_type;

typedef struct
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

#ifdef STRUCTURED_PACKET
void packet_expand_payload(packet *packet, int additional_size);

void packet_append_int(packet *packet, int data);
void packet_append_string(packet *packet, const char *data);

int packet_extract_int(packet *packet);
char *packet_extract_string(packet *packet);

#endif

#endif