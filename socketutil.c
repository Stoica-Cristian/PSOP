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

// void start_accepting_connections(int serverSocketFD)
// {
//     while (true)
//     {
//         client_connection *clientConnection = accept_incoming_connection(serverSocketFD);

//         receive_incoming_data(clientConnection);
//     }
// }

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
