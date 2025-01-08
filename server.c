#include "socketutil.h"
#include "exchange.h"
#include "utils.h"
// #include "user.h"

void receive_incoming_data(client_connection *established_connection);
void *process_packets(void *arg);
cJSON *parse_packet_payload_to_json(packet *packet);

void handle_producer_publish(int socketFD, packet *packet_received);
void handle_consumer_request(int socketFD, packet *packet_received);
void handle_subscribe(int socketFD, packet *packet_received, client_connection *established_connection);
void handle_user_authentication(int socketFD, packet *packet_received, client_connection *established_connection);

direct_exchange *direct_exch;
topic_exchange *topic_exch;

user **users;

#define MAX_PROCESSED_IDS 100

char processed_ids[MAX_PROCESSED_IDS][37]; // Stocăm până la 100 de ID-uri procesate
int processed_count = 0;

bool is_duplicate(const char *packet_id)
{
    for (int i = 0; i < processed_count; ++i)
    {
        if (strcmp(processed_ids[i], packet_id) == 0)
        {
            return true; // Pachet duplicat
        }
    }
    return false;
}
void store_packet_id(const char *packet_id)
{
    if (processed_count < MAX_PROCESSED_IDS)
    {
        strncpy(processed_ids[processed_count], packet_id, 36);
        processed_ids[processed_count][36] = '\0';
        processed_count++;
    }
}

int main()
{
    set_log_file("server.log", LOG_TRACE);

    initialize_users(&users);
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

    free_direct_exchange(direct_exch);
    free_topic_exchange(topic_exch);
    cleanup_users(users);
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

    int cnt = 0;
    while (true)
    {
        ssize_t bytes_received = recv(socketFD, &packet_received, MAX_PACKET_SIZE, 0);
        packet_received.payload[bytes_received - sizeof(packet_type) - sizeof(unique_id)] = '\0';

        char packet_id[37];
        uid_to_string(&packet_received.id, packet_id);

        if (is_duplicate(packet_id))
        {
            log_info("[process_packets(void*)] : Duplicate packet received");
            break;
        }

        printf("Received packet %d\n", ++cnt);
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
            case PKT_AUTH:
                handle_user_authentication(socketFD, &packet_received, established_connection);
                break;
            case PKT_PRODUCER_PUBLISH:
                handle_producer_publish(socketFD, &packet_received);
                break;

            case PKT_CONSUMER_REQUEST:
                handle_consumer_request(socketFD, &packet_received);
                break;

            case PKT_SUBSCRIBE:
                handle_subscribe(socketFD, &packet_received, established_connection);
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

        char *json_response_str = cJSON_PrintUnformatted(json_response);
        packet response_packet = create_packet(PKT_CONSUMER_RESPONSE, json_response_str);
        free(json_response_str);

        send_packet(socketFD, &response_packet);
        // print_packet(&response_packet);
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

        char *json_response_str = cJSON_PrintUnformatted(json_response);
        packet response_packet = create_packet(PKT_CONSUMER_RESPONSE, json_response_str);
        free(json_response_str);

        send_packet(socketFD, &response_packet);
    }
}

/*
 * Handles a subscribe request based on the JSON payload.
 *
 * @param socketFD The socket file descriptor to send the response to.
 * @param packet_received The packet containing the request details.
 */
void handle_subscribe(int socketFD, packet *packet_received, client_connection *established_connection)
{
    cJSON *json_payload = parse_packet_payload_to_json(packet_received);

    if (!json_payload)
    {
        send_bad_format_packet(socketFD, &packet_received->id);
        log_info("[handle_subscribe(int, packet*, client_connection*)] : Error parsing JSON payload");
        return;
    }

    cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

    if (!topic_item)
    {
        log_info("[handle_subscribe(int, packet*, client_connection*)] : Missing 'topic' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
        return;
    }

    const char *topic = topic_item->valuestring;

    trie_insert_subscriber(topic_exch->trie, topic, find_user(users, established_connection->username));
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

void handle_user_authentication(int socketFD, packet *packet_received, client_connection *established_connection)
{
    store_packet_id(uid_to_string_malloc(&packet_received->id));

    cJSON *json_payload = parse_packet_payload_to_json(packet_received);

    if (!json_payload)
    {
        send_bad_format_packet(socketFD, &packet_received->id);
        log_info("[handle_user_authentication(int, packet*)] : Error parsing JSON payload");
        return;
    }

    cJSON *id_item = cJSON_GetObjectItem(json_payload, "id");
    cJSON *username_item = cJSON_GetObjectItem(json_payload, "username");
    cJSON *password_item = cJSON_GetObjectItem(json_payload, "password");

    if (!username_item || !password_item)
    {
        log_info("[handle_user_authentication(int, packet*)] : Missing 'username' or 'password' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
        return;
    }

    const char *id = id_item->valuestring;
    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;

    id = id ? id : "0";

    bool authenticated = authenticate_user(users, username, password);

    if (authenticated)
    {
        packet auth_success_packet;
        auth_success_packet.packet_type = PKT_AUTH_SUCCESS;

        send_packet(socketFD, &auth_success_packet);

        log_info("[handle_user_authentication(int, packet*)] : User %s authenticated successfully", username);
    }
    else // register
    {
        bool registered = register_user(users, username, password);

        if (registered)
        {
            packet auth_success_packet;
            auth_success_packet.packet_type = PKT_AUTH_SUCCESS;

            send_packet(socketFD, &auth_success_packet);

            log_info("[handle_user_authentication(int, packet*)] : User %s registered successfully", username);
        }
        else
        {
            packet auth_failure_packet;
            auth_failure_packet.packet_type = PKT_AUTH_FAILURE;

            send_packet(socketFD, &auth_failure_packet);

            log_info("[handle_user_authentication(int, packet*)] : User %s authentication failed", username);
            return;
        }
    }

    user* this = find_user(users, username);
    this->socketFD = socketFD;
    established_connection->isAuth = authenticated;
    strncpy(established_connection->username, username, sizeof(established_connection->username) - 1);
    established_connection->username[sizeof(established_connection->username) - 1] = '\0';

    free(json_payload);
}
