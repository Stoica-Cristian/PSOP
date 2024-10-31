#include "packet.h"

packet *packet_create(packet_type type)
{
    packet *packet = malloc(sizeof(packet));

    if (!packet)
    {
        log_error("[packet_create(packet_type)] : Cannot allocate memory for packet");
        exit(EXIT_FAILURE);
    }

    packet->packet_type = type;

#ifdef STRUCTURED_PACKET
    packet->payload = malloc(INITIAL_PAYLOAD_SIZE);

    if (!packet->payload)
    {
        log_error("[packet_create(packet_type)] : Cannot allocate memory for packet payload");
        exit(EXIT_FAILURE);
    }

    packet->payload_size = 0;
    packet->payload_capacity = INITIAL_PAYLOAD_SIZE;
    packet->extraction_offset = 0;

#endif

    return packet;
}

void packet_destroy(packet *packet)
{
    if (packet)
    {
        // free(packet->payload);
        free(packet);
    }
}

void packet_set_payload(packet *packet, const char *data, int size)
{
    // packet->payload = malloc(size);
    // if (!packet->payload)
    // {
    //     log_error("[packet_set_payload(packet*, const char*, int)] : Cannot allocate memory for packet payload");
    //     exit(EXIT_FAILURE);
    // }
    memcpy(packet->payload, data, size);
}

#ifdef STRUCTURED_PACKET

void packet_expand_payload(packet *packet, int additional_size)
{
    if (packet->payload_size + additional_size > packet->payload_capacity)
    {
        int new_capacity = packet->payload_capacity + PAYLOAD_INCREMENT;
        while (new_capacity < packet->payload_size + additional_size)
        {
            new_capacity += PAYLOAD_INCREMENT;
        }
        packet->payload = realloc(packet->payload, new_capacity);
        packet->payload_capacity = new_capacity;
    }
}

void packet_append_int(packet *packet, int data)
{
    packet_expand_payload(packet, sizeof(int));
    data = htonl(data);
    memcpy(packet->payload + packet->payload_size, &data, sizeof(int));
    packet->payload_size += sizeof(int);
}

void packet_append_string(packet *packet, const char *data)
{
    int string_size = strlen(data);
    packet_append_int(packet, string_size);
    packet_expand_payload(packet, string_size);
    memcpy(packet->payload + packet->payload_size, data, string_size);
    packet->payload_size += string_size;
}

int packet_extract_int(packet *packet)
{
    if (packet->extraction_offset + sizeof(int) > packet->payload_size)
    {
        log_error("[packet_extract_int(packet*)] : Extraction offset exceeded payload size");
        exit(EXIT_FAILURE);
    }
    int data;
    memcpy(&data, packet->payload + packet->extraction_offset, sizeof(int));
    data = ntohl(data);
    packet->extraction_offset += sizeof(int);
    return data;
}

char *packet_extract_string(packet *packet)
{
    int string_size = packet_extract_int(packet);
    if (packet->extraction_offset + string_size > packet->payload_size)
    {
        log_error("[packet_extract_string(packet*)] : Extraction offset exceeded payload size");
        exit(EXIT_FAILURE);
    }
    char *data = malloc(string_size + 1);
    memcpy(data, packet->payload + packet->extraction_offset, string_size);
    data[string_size] = '\0';
    packet->extraction_offset += string_size;
    return data;
}

#endif