#include "socketutil.h"

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

//=================== Client Side ==============================

void connect_to_server(int socketFD, struct sockaddr_in *address)
{
    int result = connect(socketFD, (struct sockaddr *)address, sizeof(*address));

    if (result == 0)
    {
        printf("Connection was successfully.\n");
    }
    else
    {
        perror("[connectToServer(int, struct sockaddr_in *)] ");
        exit(EXIT_FAILURE);
    }
}

void connect_to_server_auth(int socketFD, struct sockaddr_in *address, char *username, char *password)
{
    connect_to_server(socketFD, address);

    packet* auth_packet = new_packet(PKT_AUTH, "");

    cJSON *json_auth = cJSON_CreateObject();

    unique_id id;
    uuid_clear(id.uuid);

    char id_str[37];
    uid_to_string(&id, id_str);

    cJSON_AddStringToObject(json_auth, "id", id_str);
    cJSON_AddStringToObject(json_auth, "username", username);
    cJSON_AddStringToObject(json_auth, "password", password);

    char *json_auth_str = cJSON_Print(json_auth);

    strncpy(auth_packet->payload, json_auth_str, sizeof(auth_packet->payload) - 1);
    free(json_auth_str);

    send_packet(socketFD, auth_packet);

    cJSON_Delete(json_auth);


    packet response_packet;
    response_packet.packet_type = PKT_UNKNOWN;

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

    return;
}