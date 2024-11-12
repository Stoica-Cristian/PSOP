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
        free(packet);
    }
}

void packet_set_payload(packet *packet, const char *data, int size)
{
    memcpy(packet->payload, data, size);
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

void send_bad_format_packet(int socketFD)
{
    packet bad_format_packet;
    bad_format_packet.packet_type = PKT_BAD_FORMAT;
    send_packet(socketFD, &bad_format_packet);

    log_info("[send_bad_format_packet(int)] : Bad format packet sent");
}

void send_producer_ack_packet(int socketFD)
{
    packet ack_packet;
    ack_packet.packet_type = PKT_PRODUCER_ACK;
    send_packet(socketFD, &ack_packet);

    log_info("[send_producer_ack_packet(int)] : Producer ACK packet sent");
}

void send_incomplete_packet(int socketFD)
{
    packet incomplete_packet;
    incomplete_packet.packet_type = PKT_INCOMPLETE;
    send_packet(socketFD, &incomplete_packet);

    log_info("[send_incomplete_packet(int)] : Incomplete packet sent");
}

void send_consumer_request_packet(int socketFD, const char *type, const char *identifier)
{
    if (type == NULL || identifier == NULL || strlen(type) == 0 || strlen(identifier) == 0)
    {
        log_error("[send_consumer_request_packet(int, const char*, const char*)] : Invalid type or identifier");
        return;
    }

    packet request_packet;
    request_packet.packet_type = PKT_CONSUMER_REQUEST;

    cJSON *json_payload = cJSON_CreateObject();

    if (strcmp(type, "direct") == 0)
    {
        cJSON_AddStringToObject(json_payload, "type", type);
        cJSON_AddStringToObject(json_payload, "key", identifier);
    }
    else if (strcmp(type, "topic") == 0)
    {
        cJSON_AddStringToObject(json_payload, "type", type);
        cJSON_AddStringToObject(json_payload, "topic", identifier);
    }
    else
    {
        log_error("[send_consumer_request_packet(int, const char*, const char*)] : Unknown type");
        cJSON_Delete(json_payload);
        return;
    }

    char *json_str = cJSON_PrintUnformatted(json_payload);
    strncpy(request_packet.payload, json_str, sizeof(request_packet.payload) - 1);
    cJSON_Delete(json_payload);

    send_packet(socketFD, &request_packet);

    log_info("[send_consumer_request_packet(int, const char*, const char*)] : Consumer request packet sent");
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