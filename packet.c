#include "packet.h"

packet *new_packet(packet_type type, const char *payload)
{
    packet *packet = malloc(sizeof(packet));

    if (!packet)
    {
        log_error("[new_packet(packet_type, const char*)] : %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    packet->packet_type = type;
    generate_uid(&packet->id);

    strncpy(packet->payload, payload, strlen(payload));
    packet->payload[strlen(payload)] = '\0';

    return packet;
}

packet create_packet(packet_type type, const char *payload)
{
    packet packet;
    packet.packet_type = type;
    generate_uid(&packet.id);
    printf("Packet ID: ");

    strncpy(packet.payload, payload, sizeof(packet.payload) - 1);
    packet.payload[sizeof(packet.payload) - 1] = '\0';

    return packet;
}

char *packet_type_to_string(packet_type type)
{
    switch (type)
    {
    case PKT_BAD_FORMAT:
        return "PKT_BAD_FORMAT";
    case PKT_INCOMPLETE:
        return "PKT_INCOMPLETE";
    case PKT_EMPTY_QUEUE:
        return "PKT_EMPTY_QUEUE";
    case PKT_PRODUCER_PUBLISH:
        return "PKT_PRODUCER_PUBLISH";
    case PKT_PRODUCER_ACK:
        return "PKT_PRODUCER_ACK";
    case PKT_PRODUCER_NACK:
        return "PKT_PRODUCER_NACK";
    case PKT_CONSUMER_REQUEST:
        return "PKT_CONSUMER_REQUEST";
    case PKT_CONSUMER_RESPONSE:
        return "PKT_CONSUMER_RESPONSE";
    case PKT_CONSUMER_ACK:
        return "PKT_CONSUMER_ACK";
    case PKT_CONSUMER_NACK:
        return "PKT_CONSUMER_NACK";
    case PKT_UNKNOWN:
    default:
        return "PKT_UNKNOWN";
    }
}

void packet_destroy(packet *packet)
{
    if (packet)
    {
        free(packet);
    }
}

void print_packet(const packet *packet)
{
    if (!packet)
    {
        printf("[print_packet(const packet*)] : NULL packet pointer\n");
        return;
    }

    char id_str[37];
    uid_to_string(&packet->id, id_str);

    printf("\nPacket ID: %s\n", id_str);
    printf("Packet type: %s\n", packet_type_to_string(packet->packet_type));
    printf("Packet payload: %s\n\n", packet->payload);
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
    ssize_t total_bytes_to_send = sizeof(packet_type) + strlen(packet->payload) + sizeof(packet->id);
    ssize_t bytes_sent = send(socketfd, packet, total_bytes_to_send, 0);

    if (bytes_sent == -1)
    {
        log_error("[send_packet(int, packet*)] : &s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (bytes_sent < total_bytes_to_send)
    {
        log_warn("[send_packet(int, packet*)] : incomplete packet sent (%zd of %zu bytes)", bytes_sent, total_bytes_to_send);
    }
    else if (bytes_sent == 0)
    {
        log_info("[send_packet(int, packet*)] : Connection closed by server");
    }
}

void send_bad_format_packet(int socketFD, const unique_id *uuid)
{
    packet bad_format_packet;
    bad_format_packet.packet_type = PKT_BAD_FORMAT;
    copy_uid(&bad_format_packet.id, uuid);

    send_packet(socketFD, &bad_format_packet);

    log_info("[send_bad_format_packet(int)] : Bad format packet sent");
}

void send_producer_ack_packet(int socketFD, const unique_id *uuid)
{
    packet ack_packet;
    ack_packet.packet_type = PKT_PRODUCER_ACK;

    print_uid(uuid);

    copy_uid(&ack_packet.id, uuid);
    send_packet(socketFD, &ack_packet);

    log_info("[send_producer_ack_packet(int)] : Producer ACK packet sent");
}

void send_incomplete_packet(int socketFD, const unique_id *uuid)
{
    packet incomplete_packet;
    incomplete_packet.packet_type = PKT_INCOMPLETE;
    copy_uid(&incomplete_packet.id, uuid);
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

void generate_custom_uid(char *custom_id)
{
    static const char *chars = "0123456789abcdef";
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            custom_id[i] = '-';
        }
        else
        {
            custom_id[i] = chars[rand() % 16];
        }
    }
    custom_id[36] = '\0';
}

void generate_uid(unique_id *id)
{
#if HAS_UUID
    uuid_generate(id->uuid);
#else
    generate_custom_uid(id->custom_id);
#endif
}

void uid_to_string(const unique_id *id, char *str)
{
#if HAS_UUID
    uuid_unparse(id->uuid, str);
#else
    strncpy(str, id->custom_id, 37);
#endif
}

void print_uid(const unique_id *id)
{
#if HAS_UUID
    char uuid_str[37];
    uuid_unparse(id->uuid, uuid_str);
    printf("%s\n", uuid_str);
#else
    printf("%s\n", id->custom_id);
#endif
}

void copy_uid(unique_id *dest, const unique_id *src)
{
#if HAS_UUID
    uuid_copy(dest->uuid, src->uuid);
#else
    strncpy(dest->custom_id, src->custom_id, 37);
#endif
}

bool uid_equals(const unique_id *uid1, const unique_id *uid2)
{
#if HAS_UUID
    return uuid_compare(uid1->uuid, uid2->uuid) == 0;
#else
    return strcmp(uid1->custom_id, uid2->custom_id) == 0;
#endif
}
