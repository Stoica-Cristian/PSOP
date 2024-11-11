#include "socketutil.h"
#include "exchange.h"
#include "utils.h"

void receive_incoming_data(AcceptedConnection *acceptedConnection);
void *process_packets(void *arg);
cJSON *parse_packet_payload_to_json(packet *packet);
void assign_message_to_exchange(cJSON *json_payload);

direct_exchange *direct_exch;
topic_exchange *topic_exch;

int main()
{
    set_log_file("server_log.txt", LOG_TRACE);

    direct_exch = create_direct_exchange();
    topic_exch = create_topic_exchange();

    int serverSocketFD = create_tcp_ipv4_socket();

    struct sockaddr_in *serverAddress = create_ipv4_address("", 8080);

    bind_socket(serverSocketFD, serverAddress);

    listen_for_connections(serverSocketFD);

    while (true)
    {
        AcceptedConnection *clientConnection = accept_incoming_connection(serverSocketFD);

        receive_incoming_data(clientConnection);
    }

    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void receive_incoming_data(AcceptedConnection *acceptedConnection)
{
    pthread_t id;
    pthread_create(&id, NULL, process_packets, (void *)acceptedConnection);
}

void *process_packets(void *arg)
{
    AcceptedConnection *acceptedConnection = (AcceptedConnection *)arg;
    int socketFD = acceptedConnection->acceptedSocketFD;

    packet packet_received;
    packet_received.packet_type = PKT_UNKNOWN;

    while (true)
    {
        ssize_t bytes_received = recv(socketFD, &packet_received, MAX_PACKET_SIZE, 0);

        if (bytes_received == 0)
        {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(acceptedConnection->address.sin_addr), ip_str, INET_ADDRSTRLEN);
            log_info("[process_packets(void*)] : Connection closed by client with address %s and port %d",
                     ip_str, ntohs(acceptedConnection->address.sin_port));
            break;
        }

        if (bytes_received < 0)
        {
            log_error("[process_packets(void*)] : %s", strerror(errno));
            break;
        }

        if (bytes_received > 0)
        {
            packet_received.payload[bytes_received - sizeof(packet_type)] = '\0';
            cJSON *json_payload = parse_packet_payload_to_json(&packet_received);

            if (!json_payload)
            {
                packet nack;
                nack.packet_type = PKT_PRODUCER_NACK;
                send_packet(socketFD, &nack);
                log_info("[process_packets(void*)] : NACK sent for bad JSON formatting");
                continue;
            }
            else
            {
                packet ack;
                ack.packet_type = PKT_PRODUCER_ACK;
                send_packet(socketFD, &ack);
                log_info("[process_packets(void*)] : ACK sent");
            }

            switch (packet_received.packet_type)
            {
            case PKT_PRODUCER_PUBLISH:
                printf("%s - %s\n", packet_received.packet_type == 1 ? "PKT_PRODUCER_PUBLISH" : "PKT_UNKNOWN", packet_received.payload);

                // maybe on another thread
                assign_message_to_exchange(json_payload);
                break;

            case PKT_UNKNOWN:
            default:
                log_info("[process_packets(void*)] : Packet unknown");
            }
        }
    }

    close(socketFD);
    return NULL;
}

cJSON *parse_packet_payload_to_json(packet *packet)
{
    if (!packet)
    {
        log_debug("[parse_packet_payload_to_json(packet*)] : NULL packet pointer");
        return NULL;
    }

    cJSON *json_payload = cJSON_Parse(packet->payload);

    if (json_payload == NULL)
    {
        log_debug("[parse_packet_payload_to_json(packet*)] : Error parsing JSON payload");
        return NULL;
    }

    return json_payload;
}

/**
 * Assigns a message to the appropriate exchange based on the JSON payload.
 *
 * @param json_payload The JSON payload containing the message details.
 */
void assign_message_to_exchange(cJSON *json_payload)
{
    cJSON *type_item = cJSON_GetObjectItem(json_payload, "type");
    cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

    if (!payload_item)
    {
        log_error("[assign_message_to_exchange(cJSON*)] : Missing 'payload' in JSON payload");
        return;
    }
    if (!type_item)
    {
        log_error("[assign_message_to_exchange(cJSON*)] : Missing 'type' in JSON payload");
        return;
    }

    const char *type = type_item->valuestring;
    const char *payload = payload_item->valuestring;

    message message;
    strncpy(message.payload, payload, sizeof(message.payload) - 1);
    message.payload[sizeof(message.payload) - 1] = '\0';

    if (strcmp(type, "direct") == 0)
    {
        message.type = MSG_DIRECT;

        cJSON *key_item = cJSON_GetObjectItem(json_payload, "key");

        if (!key_item || !key_item->valuestring)
        {
            log_error("[assign_message_to_exchange(cJSON*)] : 'key' is NULL");
            return;
        }
        const char *key = key_item->valuestring;

        direct_exch_insert_message(direct_exch, message, key);
        direct_exch_print_queues(direct_exch);
    }

    if (strcmp(type, "topic") == 0)
    {
        cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

        if (!topic_item || !topic_item->valuestring)
        {
            log_error("[assign_message_to_exchange(cJSON*)] : 'topic' is NULL");
            return;
        }

        const char *topic = topic_item->valuestring;
        message.type = MSG_TOPIC;

        topic_exch_insert_topic(topic_exch, topic);
        topic_exch_print_trie(topic_exch);
    }
}
