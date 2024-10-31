#include "socketutil.h"
#include "packet.h"

char **read_and_parse_json_from_file(const char *filename, int *message_count);
void send_packet(int socketfd, packet *packet);

int main()
{
    set_log_file("client_log.txt", LOG_TRACE);

    int socketFD = create_tcp_ipv4_socket();
    struct sockaddr_in *address = create_ipv4_address("127.0.0.1", 8080);

    connect_to_server(socketFD, address);

    //======================== test ===========================//

    int message_count;

    char **messages = read_and_parse_json_from_file("messages.json", &message_count);

    if (messages)
    {
        for (int i = 0; i < message_count; i++)
        {
            packet packet;
            packet.packet_type = PKT_PRODUCER_PUBLISH;
            strcpy(packet.payload, messages[i]);
            send_packet(socketFD, &packet);

            log_trace("Message %d: %s", i + 1, packet.payload);
            free(messages[i]);

            sleep(1);
            // TO DO: ACK
        }
        free(messages);
    }
    //========================================================//

    while (1)
    {
        sleep(10);
    }

    close(socketFD);

    return 0;
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

char *create_json_message(const char *binding_key, const char *payload)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
        return NULL;

    cJSON_AddStringToObject(json, "binding_key", binding_key);
    cJSON_AddStringToObject(json, "payload", payload);

    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    return json_string;
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
