#include "message_broker.h"

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

int socketFD;
struct sockaddr_in *address;

void connect_to_server()
{
    socketFD = create_tcp_ipv4_socket();
    address = create_ipv4_address("127.0.0.1", 8080);

    int result = connect(socketFD, (struct sockaddr *)address, sizeof(*address));

    if (result == 0)
    {
        set_log_file("client.log", LOG_TRACE);
        log_info("[connectToServer(int, struct sockaddr_in *)] : Connection was successfully.");
    }
    else
    {
        log_fatal("[connectToServer(int, struct sockaddr_in *)] : %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void connect_to_server_auth(char *username, char *password)
{
    connect_to_server();

    packet *auth_packet = new_packet(PKT_AUTH, "");

    cJSON *json_auth = cJSON_CreateObject();

    cJSON_AddStringToObject(json_auth, "username", username);
    cJSON_AddStringToObject(json_auth, "password", password);

    char *json_auth_str = cJSON_Print(json_auth);

    strncpy(auth_packet->payload, json_auth_str, sizeof(auth_packet->payload) - 1);
    free(json_auth_str);

    client_send_packet(auth_packet);

    cJSON_Delete(json_auth);

    packet response_packet = create_packet(PKT_UNKNOWN, "");

    ssize_t bytes_received = recv(socketFD, &response_packet, MAX_PACKET_SIZE, 0);

    if (bytes_received == 0)
    {
        log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Connection closed by server");
        return;
    }

    if (bytes_received < 0)
    {
        log_error("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : %s", strerror(errno));
        return;
    }

    if (bytes_received > 0)
    {
        switch (response_packet.packet_type)
        {
        case PKT_AUTH_SUCCESS:
            log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Auth success");
            break;
        case PKT_AUTH_FAILURE:
            log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Auth failure");
            break;
        case PKT_UNKNOWN:
        default:
            log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Packet unknown");
        }
    }

    pthread_t id;
    pthread_create(&id, NULL, process_packets, NULL);
}

bool ack_received = false;
unique_id current_id;
int ack_count = 0;

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
            packet.payload[bytes_received - sizeof(packet_type) - sizeof(unique_id)] = '\0';

            switch (packet.packet_type)
            {
            case PKT_PRODUCER_ACK:
                if (uid_equals(&packet.id, &current_id))
                {
                    ack_received = true;

                    char str[37];
                    uid_to_string(&packet.id, str);
                    log_info("[process_packets(void*)] : Received ACK for packet with ID: %s", str);
                }
                else
                {
                    log_warn("[process_packets(void*)] : Received ACK for unknown packet");
                }
                break;

            case PKT_CONSUMER_RESPONSE:
                print_packet(&packet);
                handle_consumer_response(packet);
                break;
            case PKT_QUEUE_NOT_FOUND:
                log_info("[process_packets(void*)] : Queue not found");
                break;

            case PKT_BAD_FORMAT:
                log_error("[process_packets(void*)] : Bad format packet received");
                break;

            case PKT_INCOMPLETE:
                log_error("[process_packets(void*)] : Incomplete packet received");
                break;

            case PKT_AUTH_SUCCESS:
                log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Auth success");
                break;

            case PKT_AUTH_FAILURE:
                log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Auth failure");
                break;

            case PKT_UNKNOWN:
            default:
                log_info("[process_packets(void*)] : Packet unknown");
            }
        }
    }

    return NULL;
}

void handle_consumer_response(packet received_packet)
{
    cJSON *json_payload = cJSON_Parse(received_packet.payload);

    cJSON *type_item = cJSON_GetObjectItem(json_payload, "type");

    if (!type_item)
    {
        log_info("[handle_consumer_response(packet)] : Missing 'type' in JSON payload");
        return;
    }

    const char *type = type_item->valuestring;

    if (strcmp(type, "direct") == 0)
    {
        cJSON *key_item = cJSON_GetObjectItem(json_payload, "key");

        if (!key_item)
        {
            log_info("[handle_consumer_response(packet)] : Missing 'key' in JSON payload");
            return;
        }

        const char *key = key_item->valuestring;

        cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

        if (!payload_item)
        {
            log_info("[handle_consumer_response(packet)] : Missing 'payload' in JSON payload");
            return;
        }

        const char *payload = payload_item->valuestring;

        log_info("[handle_consumer_response(packet)] : Received payload for key %s : %s", key, payload);
    }

    if (strcmp(type, "topic") == 0)
    {
        cJSON *topic_item = cJSON_GetObjectItem(json_payload, "topic");

        if (!topic_item)
        {
            log_info("[handle_consumer_response(packet)] : Missing 'topic' in JSON payload");
            return;
        }

        // const char *topic = topic_item->valuestring;

        cJSON *payload_item = cJSON_GetObjectItem(json_payload, "payload");

        if (!payload_item)
        {
            log_info("[handle_consumer_response(packet)] : Missing 'payload' in JSON payload");
            return;
        }

        // const char *payload = payload_item->valuestring;
        // log_info("[handle_consumer_response(packet)] : Received payload for topic %s : %s", topic, payload);
    }

    cJSON_Delete(json_payload);
}

// packet functions

void client_send_packet(packet *packet)
{
    if (!packet)
    {
        log_warn("[client_send_packet(int, packet*)] : NULL packet pointer");
        return;
    }
    ssize_t total_bytes_to_send = sizeof(packet_type) + strlen(packet->payload) + sizeof(packet->id);
    ssize_t bytes_sent = send(socketFD, packet, total_bytes_to_send, 0);

    if (bytes_sent == -1)
    {
        log_error("[client_send_packet(int, packet*)] : &s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (bytes_sent < total_bytes_to_send)
    {
        log_warn("[client_send_packet(int, packet*)] : incomplete packet sent (%zd of %zu bytes)", bytes_sent, total_bytes_to_send);
    }
    else if (bytes_sent == 0)
    {
        log_info("[client_send_packet(int, packet*)] : Connection closed by server");
    }
}

void send_request_packet(const char *type, const char *identifier)
{
    if (type == NULL || identifier == NULL || strlen(type) == 0 || strlen(identifier) == 0)
    {
        log_error("[send_request_packet(int, const char*, const char*)] : Invalid type or identifier");
        return;
    }

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
        log_error("[send_request_packet(int, const char*, const char*)] : Unknown type");
        cJSON_Delete(json_payload);
        return;
    }

    char *json_str = cJSON_PrintUnformatted(json_payload);
    packet request_packet = create_packet(PKT_CONSUMER_REQUEST, json_str);

    cJSON_Delete(json_payload);

    client_send_packet(&request_packet);

    log_info("[send_request_packet(int, const char*, const char*)] : Consumer request packet sent");

    packet response_packet = create_packet(PKT_UNKNOWN, "");

    ssize_t bytes_received = recv(socketFD, &response_packet, MAX_PACKET_SIZE, 0);

    if (bytes_received == 0)
    {
        log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Connection closed by server");
        return;
    }

    if (bytes_received < 0)
    {
        log_error("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : %s", strerror(errno));
        return;
    }

    if (bytes_received > 0)
    {
        switch (response_packet.packet_type)
        {
        case PKT_CONSUMER_RESPONSE:
            print_packet(&response_packet);
            handle_consumer_response(response_packet);
            break;

        case PKT_UNKNOWN:
        default:
            log_info("[connectToServerAuth(int, struct sockaddr_in *, char *, char *)] : Packet unknown");
        }
    }
}

void send_disconnect_packet()
{
    packet disconnect_packet = create_packet(PKT_DISCONNECT, "");
    client_send_packet(&disconnect_packet);

    log_info("[send_disconnect_packet(int)] : Disconnect packet sent");
}

void send_subscribe_packet(const char *topic)
{
    if (topic == NULL || strlen(topic) == 0)
    {
        log_error("[send_subscribe_packet(int, const char*)] : Invalid topic");
        return;
    }

    cJSON *json_payload = cJSON_CreateObject();
    cJSON_AddStringToObject(json_payload, "topic", topic);

    char *json_str = cJSON_PrintUnformatted(json_payload);
    packet subscribe_packet = create_packet(PKT_SUBSCRIBE, json_str);

    cJSON_Delete(json_payload);

    client_send_packet(&subscribe_packet);

    log_info("[send_subscribe_packet(int, const char*)] : Subscribe packet sent");
}

void send_messages_from_json_file(const char *filename)
{
    int message_count;
    char **messages = read_and_parse_json_from_file(filename, &message_count);

    if (messages)
    {
        for (int i = 0; i < message_count; i++)
        {
            packet *packet = new_packet(PKT_PRODUCER_PUBLISH, messages[i]);

            print_packet(packet);

            current_id = packet->id;
            ack_received = false;

            int retries = 0;
            while (retries < MAX_RETRIES)
            {
                // printf("Sending packet: ");
                // print_uid(&current_id);
                // print_packet(packet);

                client_send_packet(packet);

                usleep(RETRY_DELAY * (retries + 1));
                
                if (ack_received)
                {
                    ack_count++;
                    break;
                }

                retries++;
                log_warn("No ACK received for packet %d, retrying (%d/%d)", i + 1, retries, MAX_RETRIES);
            }

            if (!ack_received)
            {
                log_error("Failed to send packet %d after %d retries", i + 1, MAX_RETRIES);
            }

            free(messages[i]);
            free(packet);
        }

        printf("\n");
        log_info("Sent %d packets, received %d acks\n", message_count, ack_count);

        free(messages);
    }
}

char **read_and_parse_json_from_file(const char *filename, int *message_count)
{
    int fd = open(filename, O_RDONLY);

    if (fd < 0)
    {
        log_error("[read_and_send_messages(const char*)] : &s", strerror(errno));
        return NULL;
    }

    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char *buffer = malloc(length + 1);

    ssize_t bytes_read = read(fd, buffer, length);

    if (bytes_read < 0)
    {
        perror("[read_and_send_messages(const char*)] ");
        free(buffer);
        close(fd);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    close(fd);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json)
    {
        fprintf(stderr, "[read_and_send_messages(const char*)] : Failed to parse JSON\n");
        return NULL;
    }

    cJSON *messages = cJSON_GetObjectItem(json, "messages");

    *message_count = cJSON_GetArraySize(messages);
    char **message_array = malloc((*message_count) * sizeof(char *));

    int i = 0;
    cJSON *message;

    cJSON_ArrayForEach(message, messages)
    {
        char *payload = cJSON_PrintUnformatted(message);
        message_array[i++] = payload;
    }

    cJSON_Delete(json);
    return message_array;
}
