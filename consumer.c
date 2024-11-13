#include "socketutil.h"
#include "utils.h"

char *generate_json_request_key(const char *key);
char *generate_json_request_topic(const char *topic);
void *process_packets(void *);
void handle_consumer_ack(packet packet);

int socketFD;
struct sockaddr_in *address;

int main()
{
    set_log_file("consumer_log.txt", LOG_TRACE);

    socketFD = create_tcp_ipv4_socket();
    address = create_ipv4_address("127.0.0.1", 8080);

    connect_to_server(socketFD, address);

    pthread_t id;
    pthread_create(&id, NULL, process_packets, NULL);

    send_consumer_request_packet(socketFD, "direct", "key1");
    send_consumer_request_packet(socketFD, "topic", "livingroom.temperature");

    pthread_join(id, NULL);

    close(socketFD);
    free(address);

    return 0;
}

void *process_packets(void *)
{
    packet packet;
    packet.packet_type = PKT_UNKNOWN;

    while (true)
    {
        ssize_t bytes_received = recv(socketFD, &packet, MAX_PACKET_SIZE, 0);

        if (bytes_received == 0)
        {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address->sin_addr), ip_str, INET_ADDRSTRLEN);
            log_info("[process_packets(void*)] : Connection closed by server with address %s and port %d",
                     ip_str, ntohs(address->sin_addr.s_addr));
            break;
        }

        if (bytes_received < 0)
        {
            log_error("[process_packets(void*)] : %s", strerror(errno));
            break;
        }

        if (bytes_received > 0)
        {
            packet.payload[bytes_received - sizeof(packet_type)] = '\0';

            switch (packet.packet_type)
            {
            case PKT_CONSUMER_RESPONSE:
                handle_consumer_ack(packet);
                break;

            case PKT_UNKNOWN:
            default:
                log_info("[process_packets(void*)] : Packet unknown");
            }
        }
    }

    return NULL;
}

char *generate_json_request_key(const char *key)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", "direct");
    cJSON_AddStringToObject(json, "key", key);

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    return json_str;
}

char *generate_json_request_topic(const char *topic)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", "topic");
    cJSON_AddStringToObject(json, "topic", topic);

    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    return json_str;
}

void handle_consumer_ack(packet packet)
{
    cJSON *json_payload = cJSON_Parse(packet.payload);

    cJSON *type_item = cJSON_GetObjectItem(json_payload, "type");

    if (!type_item)
    {
        log_info("[handle_consumer_ack(packet)] : Missing 'type' in JSON payload");
        return;
    }

    const char *type = type_item->valuestring;

    if (strcmp(type, "direct") == 0)
    {
        cJSON *key_item = cJSON_GetObjectItem(json_payload, "key");

        if (!key_item)
        {
            log_info("[handle_consumer_ack(packet)] : Missing 'key' in JSON payload");
            return;
        }

        const char *key = key_item->valuestring;

        cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

        if (!payload_item)
        {
            log_info("[handle_consumer_ack(packet)] : Missing 'payload' in JSON payload");
            return;
        }

        const char *payload = payload_item->valuestring;

        log_info("[handle_consumer_ack(packet)] : Received payload for key %s : %s", key, payload);
    }

    if (strcmp(type, "topic") == 0)
    {
        cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

        if (!topic_item)
        {
            log_info("[handle_consumer_ack(packet)] : Missing 'topic' in JSON payload");
            return;
        }

        const char *topic = topic_item->valuestring;

        cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

        if (!payload_item)
        {
            log_info("[handle_consumer_ack(packet)] : Missing 'payload' in JSON payload");
            return;
        }

        const char *payload = payload_item->valuestring;

        log_info("[handle_consumer_ack(packet)] : Received payload for topic %s : %s", topic, payload);
    }

    cJSON_Delete(json_payload);
}

