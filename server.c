#include "server.h"

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

// server functions

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

    packet packet_received = create_packet(PKT_UNKNOWN, "");

    int cnt = 0;
    while (true)
    {
        if (established_connection->acceptedSocketFD == -1)
        {
            break;
        }

        ssize_t bytes_received = recv(socketFD, &packet_received, MAX_PACKET_SIZE, 0);
        packet_received.payload[bytes_received - sizeof(packet_type) - sizeof(unique_id)] = '\0';

        // char packet_id[37];
        // uid_to_string(&packet_received.id, packet_id);

        // if (is_duplicate(packet_id))
        // {
        //     log_info("[process_packets(void*)] : Duplicate packet received");
        //     continue;
        // }

        // dupa inchiderea clientului folosind CTRL+C, serverul primeste un pachet identic cu ultimul primit, dar cu PacketType PKT_UNKNOWN
        if (packet_received.packet_type == PKT_UNKNOWN)
        {
            log_info("[process_packets(void*)] : Duplicate packet received");
            handle_disconnect(socketFD, established_connection);
            continue;
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

            case PKT_DISCONNECT:
                log_info("[process_packets(void*)] : Disconnect packet received");
                handle_disconnect(socketFD, established_connection);
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

/**
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
 * Handles a user authentication request based on the JSON payload.
 *
 * @param socketFD The socket file descriptor to send the response to.
 * @param packet_received The packet containing the request details.
 * @param established_connection The client connection object.
 */
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

    cJSON *username_item = cJSON_GetObjectItem(json_payload, "username");
    cJSON *password_item = cJSON_GetObjectItem(json_payload, "password");

    if (!username_item || !password_item)
    {
        log_info("[handle_user_authentication(int, packet*)] : Missing 'username' or 'password' in JSON payload");
        send_incomplete_packet(socketFD, &packet_received->id);
        return;
    }

    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;

    bool authenticated = authenticate_user(users, username, password);

    if (authenticated)
    {
        packet auth_success_packet;
        auth_success_packet.packet_type = PKT_AUTH_SUCCESS;

        send_packet(socketFD, &auth_success_packet);

        log_info("[handle_user_authentication(int, packet*)] : User %s authenticated successfully", username);

        print_users(users);
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

            print_users(users);
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

    user *this = find_user(users, username);
    this->socketFD = socketFD;
    established_connection->isAuth = authenticated;
    strncpy(established_connection->username, username, sizeof(established_connection->username) - 1);
    established_connection->username[sizeof(established_connection->username) - 1] = '\0';

    free(json_payload);
}

/**
 * Handles a disconnect request.
 *
 * @param socketFD The socket file descriptor to send the response to.
 * @param established_connection The client connection object.
 */
void handle_disconnect(int socketFD, client_connection *established_connection)
{
    log_info("[handle_disconnect(int, client_connection*)] : Disconnecting user %s", established_connection->username);

    user *this = find_user(users, established_connection->username);
    this->socketFD = -1;

    established_connection->isAuth = false;
    established_connection->acceptedSocketFD = -1;
    established_connection->acceptedSuccessfully = false;
    established_connection->error = 0;

    close(socketFD);
    free(established_connection);
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

//====================== socket functions ======================

int create_tcp_ipv4_socket()
{
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFD < 0)
    {
        log_fatal("[create_tcp_ipv4_socket(void)] : %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return socketFD;
}

struct sockaddr_in *create_ipv4_address(char *ip, int port)
{
    struct sockaddr_in *addr = malloc(sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    if (strlen(ip) == 0)
    {
        addr->sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        if (inet_pton(AF_INET, ip, &addr->sin_addr.s_addr) <= 0)
        {
            log_fatal("[create_ipv4_address(char*, int)] : %s", strerror(errno));

            free(addr);
            exit(EXIT_FAILURE);
        }
    }

    return addr;
}

void bind_socket(int serverSocketFD, struct sockaddr_in *address)
{
    int result = bind(serverSocketFD, (struct sockaddr *)address, sizeof(*address));

    if (result == 0)
    {
        log_trace("[bindSocket(int, struct sockaddr_in *)] : Socket was bound successfully");
    }
    else
    {
        log_fatal("[bindSocket(int, struct sockaddr_in *)] : %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void listen_for_connections(int serverSocketFD)
{
    int result = listen(serverSocketFD, 10);

    if (result == 0)
    {
        log_trace("[listenForConnections(int)] : Socket is listening...");
    }
    else
    {
        log_fatal("[listenForConnections(int)] : %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

client_connection *accept_incoming_connection(int serverSocketFD)
{
    struct sockaddr_in clientAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr_in);

    int clientSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressSize);

    client_connection *acceptedConnection = malloc(sizeof(client_connection));

    acceptedConnection->acceptedSocketFD = clientSocketFD;
    acceptedConnection->address = clientAddress;
    acceptedConnection->acceptedSuccessfully = (clientSocketFD >= 0);
    acceptedConnection->error = clientSocketFD >= 0 ? 0 : clientSocketFD;
    acceptedConnection->isAuth = false;

    return acceptedConnection;
}

void receive_incoming_data(client_connection *established_connection)
{
    pthread_t id;
    pthread_create(&id, NULL, process_packets, (void *)established_connection);
}

//====================== packet functions ======================

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

void send_queue_not_found_packet(int socketFD, const unique_id *uuid)
{
    packet queue_not_found_packet;
    queue_not_found_packet.packet_type = PKT_QUEUE_NOT_FOUND;
    copy_uid(&queue_not_found_packet.id, uuid);
    send_packet(socketFD, &queue_not_found_packet);

    log_info("[send_queue_not_found_packet(int)] : Queue not found packet sent");
}

// check if a packet ID is a duplicate

bool is_duplicate(const char *packet_id)
{
    for (int i = 0; i < processed_count; ++i)
    {
        if (strcmp(processed_ids[i], packet_id) == 0)
        {
            return true;
        }
    }
    return false;
}

void store_packet_id(const char *packet_id)
{
    if (processed_count >= MAX_PROCESSED_IDS)
    {
        processed_count = 0;
    }
    strncpy(processed_ids[processed_count], packet_id, 36);
    processed_ids[processed_count][36] = '\0';
    processed_count++;
}
