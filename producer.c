#include "socketutil.h"
#include "utils.h"

char **read_and_parse_json_from_file(const char *filename, int *message_count);
void *process_packets(void *);

int socketFD;
struct sockaddr_in *address;

int main()
{
    set_log_file("producer_log.txt", LOG_TRACE);

    socketFD = create_tcp_ipv4_socket();
    address = create_ipv4_address("127.0.0.1", 8080);

    connect_to_server(socketFD, address);

    //======================== test ===========================//

    int message_count;

    char **messages = read_and_parse_json_from_file("messages.json", &message_count);

    if (messages)
    {
        pthread_t id;
        pthread_create(&id, NULL, process_packets, NULL);

        for (int i = 0; i < message_count; i++)
        {
            packet packet;
            packet.packet_type = PKT_PRODUCER_PUBLISH;
            strcpy(packet.payload, messages[i]);
            send_packet(socketFD, &packet);

            log_trace("Message %d: %s", i + 1, packet.payload);
            free(messages[i]);

            usleep(100000); // 100ms
        }
        free(messages);
        pthread_join(id, NULL);
    }
    //========================================================//

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
            case PKT_PRODUCER_ACK:
                log_info("[process_packets(void*)] : ACK received");
                break;

            case PKT_UNKNOWN:
            default:
                log_info("[process_packets(void*)] : Packet unknown");
            }
        }
    }

    return NULL;
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
