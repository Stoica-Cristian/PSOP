#include "socketutil.h"
#include "exchange.h"
#include "packet.h"

void receive_incoming_data(AcceptedConnection *acceptedConnection);
void *process_packets(void *arg);
cJSON *parse_packet_payload_to_json(packet *packet);
void assign_message_to_exchange(cJSON *json_payload);

direct_exchange *direc_exch;

int main()
{
    set_log_file("server_log.txt", LOG_TRACE);

    direc_exch = create_direct_exchange();

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

    packet packet;
    packet.packet_type = PKT_UNKNOWN;

    while (true)
    {
        ssize_t bytes_received = recv(socketFD, &packet, MAX_PACKET_SIZE, 0);

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
            packet.payload[bytes_received - sizeof(packet_type)] = '\0';
            cJSON *json_payload = parse_packet_payload_to_json(&packet);

            if (!json_payload)
            {
                // send NACK for bad json formatting
                // log and continue
            }

            switch (packet.packet_type)
            {
            case PKT_PRODUCER_PUBLISH:
                printf("%s - %s\n", packet.packet_type == 1 ? "PKT_PRODUCER_PUBLISH" : "PKT_UNKNOWN", packet.payload);

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

void assign_message_to_exchange(cJSON *json_payload)
{
    const char *type = cJSON_GetObjectItem(json_payload, "type")->valuestring;
    const char *key = cJSON_GetObjectItem(json_payload, "key")->valuestring;
    // const char *topic = cJSON_GetObjectItem(json_payload, "topic")->valuestring;
    const char *payload = cJSON_GetObjectItem(json_payload, "payload")->valuestring;

    message message;

    if (strcmp(type, "direct") == 0)
    {
        message.type = MSG_DIRECT;
        strcpy(message.payload, payload);

        exch_insert_message(direc_exch, message, key);


        queue *q = exch_get_queue(direc_exch, key);
        print_queue(q);
    }
}