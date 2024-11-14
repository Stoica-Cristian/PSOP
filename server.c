#include "socketutil.h"
#include "exchange.h"
#include "utils.h"

void receive_incoming_data(client_connection *established_connection);
void *process_packets(void *arg);
cJSON *parse_packet_payload_to_json(packet *packet);
void assign_message_to_exchange(cJSON *json_payload);

void handle_producer_publish(int socketFD, packet *packet_received);
void handle_consumer_request(int socketFD, packet *packet_received);

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
        client_connection *clientConnection = accept_incoming_connection(serverSocketFD);

        receive_incoming_data(clientConnection);
    }

    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void receive_incoming_data(client_connection *established_connection)
{
    pthread_t id;
    pthread_create(&id, NULL, process_packets, (void *)established_connection);
}

/**
 * Processes incoming packets from a client.
 *
 * @param arg The client_connection object containing the socket file descriptor and client address.
 * @return NULL
 */
void *process_packets(void *arg)
{
    client_connection *established_connection = (client_connection *)arg;
    int socketFD = established_connection->acceptedSocketFD;

    packet packet_received;
    packet_received.packet_type = PKT_UNKNOWN;

    while (true)
    {
        ssize_t bytes_received = recv(socketFD, &packet_received, MAX_PACKET_SIZE, 0);

        print_packet(&packet_received);

        if (bytes_received == 0)
        {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(established_connection->address.sin_addr), ip_str, INET_ADDRSTRLEN);
            log_info("[process_packets(void*)] : Connection closed by client with address %s and port %d",
                     ip_str, ntohs(established_connection->address.sin_port));
            break;
        }

        if (bytes_received < 0)
        {
            log_error("[process_packets(void*)] : %s", strerror(errno));
            break;
        }

        if (bytes_received > 0)
        {
            switch (packet_received.packet_type)
            {
            case PKT_PRODUCER_PUBLISH:
                handle_producer_publish(socketFD, &packet_received);
                break;

            case PKT_CONSUMER_REQUEST:
                handle_consumer_request(socketFD, &packet_received);
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

/**
 * Handles a producer publish request based on the JSON payload.
 *
 * @param socketFD The socket file descriptor to send the response to.
 * @param packet_received The packet containing the request details.
 */
void handle_producer_publish(int socketFD, packet *packet_received)
{
    cJSON *json_payload = parse_packet_payload_to_json(packet_received);

    if (!json_payload)
    {
        send_bad_format_packet(socketFD, &packet_received->id);
        log_info("[handle_producer_publish(int, packet*)] : Error parsing JSON payload");
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(json_payload, "type");
    cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

    if (!payload_item)
    {
        log_info("[handle_producer_publish(int, packet*)] : Missing 'payload' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
        return;
    }
    if (!type_item)
    {
        log_info("[handle_producer_publish(int, packet*)] : Missing 'type' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
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
            log_info("[handle_producer_publish(int, packet*)] : 'key' is NULL");
            send_incomplete_packet(socketFD, &packet_received->id);
            return;
        }
        const char *key = key_item->valuestring;

        send_producer_ack_packet(socketFD, &packet_received->id);

        direct_exch_insert_message(direct_exch, message, key);
        direct_exch_print_queues(direct_exch);
    }

    if (strcmp(type, "topic") == 0)
    {
        cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

        if (!topic_item || !topic_item->valuestring)
        {
            log_info("[handle_producer_publish(int, packet*)] : 'topic' is NULL");
            send_incomplete_packet(socketFD, &packet_received->id);
            return;
        }

        const char *topic = topic_item->valuestring;
        message.type = MSG_TOPIC;

        send_producer_ack_packet(socketFD, &packet_received->id);

        topic_exch_insert_message(topic_exch, message, topic);
        topic_exch_print_trie(topic_exch);
    }
}

/**
 * Handles a consumer request based on the JSON payload.
 *
 * @param socketFD The socket file descriptor to send the response to.
 * @param packet_received The packet containing the request details.
 */
void handle_consumer_request(int socketFD, packet *packet_received)
{
    cJSON *json_payload = parse_packet_payload_to_json(packet_received);

    if (!json_payload)
    {
        send_bad_format_packet(socketFD, &packet_received->id);
        log_info("[handle_consumer_request(int, cJSON*)] : Error parsing JSON payload");
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(json_payload, "type");

    if (!type_item)
    {
        log_info("[handle_consumer_request(int, cJSON*)] : Missing 'type' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
        return;
    }

    const char *type = type_item->valuestring;

    if (strcmp(type, "direct") == 0)
    {
        cJSON *key_item = cJSON_GetObjectItem(json_payload, "key");

        if (!key_item)
        {
            log_info("[handle_consumer_request(int, cJSON*)] : Missing 'key' in JSON payload");
            send_incomplete_packet(socketFD, &packet_received->id);
            return;
        }
        const char *key = key_item->valuestring;

        queue *q = direct_exch_get_queue(direct_exch, key);

        if (!q)
        {
            log_info("[handle_consumer_request(int, cJSON*)] : Queue not found for key %s", key);
            return;
        }

        message msg = dequeue_message(q);

        // until implementation of queue condition variable
        if (msg.type == MSG_EMPTY)
        {
            log_info("[handle_consumer_request(int, cJSON*)] : No messages in queue for key %s", key);
            return;
        }

        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddStringToObject(json_response, "type", "direct");
        cJSON_AddStringToObject(json_response, "key", key);
        cJSON_AddStringToObject(json_response, "payload", msg.payload);

        packet response_packet;
        response_packet.packet_type = PKT_CONSUMER_RESPONSE;

        char *json_response_str = cJSON_PrintUnformatted(json_response);
        strncpy(response_packet.payload, json_response_str, sizeof(response_packet.payload) - 1);
        free(json_response_str);

        send_packet(socketFD, &response_packet);
    }

    if (strcmp(type, "topic") == 0)
    {
        cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

        if (!topic_item)
        {
            log_info("[handle_consumer_request(int, cJSON*)] : Missing 'topic' in JSON payload");
            send_incomplete_packet(socketFD, &packet_received->id);
            return;
        }

        const char *topic = topic_item->valuestring;

        queue *q = topic_exch_search_topic(topic_exch, topic);

        if (!q)
        {
            log_info("[handle_consumer_request(int, cJSON*)] : Queue not found for topic %s", topic);
            return;
        }

        message msg = dequeue_message(q);

        // until implementation of queue condition variable
        if (msg.type == MSG_EMPTY)
        {
            log_error("[handle_consumer_request(int, cJSON*)] : No messages in queue for topic %s", topic);
            return;
        }

        cJSON *json_response = cJSON_CreateObject();
        cJSON_AddStringToObject(json_response, "type", "topic");
        cJSON_AddStringToObject(json_response, "topic", topic);
        cJSON_AddStringToObject(json_response, "payload", msg.payload);

        packet response_packet;
        response_packet.packet_type = PKT_CONSUMER_RESPONSE;

        char *json_response_str = cJSON_PrintUnformatted(json_response);
        strncpy(response_packet.payload, json_response_str, sizeof(response_packet.payload) - 1);
        free(json_response_str);

        send_packet(socketFD, &response_packet);
    }
}

/**
 * Parses the payload of a packet to a JSON object.
 *
 * @param packet The packet to parse.
 * @return The JSON object or NULL if an error occurred.
 */
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