#include "utils.h"

void set_log_file(const char *filename, int level)
{
    FILE *log_file = fopen(filename, "a");

    if (log_file == NULL)
    {
        perror("[set_log_file(const char*)] ");
        return;
    }

    log_add_fp(log_file, level);
}


void send_data_from_terminal(int socketFD)
{
    char *line = NULL;
    size_t lineSize = 0;

    while (true)
    {
        ssize_t rc = getline(&line, &lineSize, stdin);
        line[rc - 1] = 0;

        if (rc > 0)
        {
            if (strncmp(line, "exit", 4) == 0)
                break;

            send(socketFD, line, strlen(line), 0);
        }
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
