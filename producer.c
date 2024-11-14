#include "socketutil.h"
#include "utils.h"

#define MAX_RETRIES 3
#define RETRY_DELAY 100000 // 100ms

void *process_packets(void *);

int socketFD;
struct sockaddr_in *address;

bool ack_received = false;
unique_id current_id;
int ack_count = 0;

int main()
{
    set_log_file("producer_log.txt", LOG_TRACE);

    socketFD = create_tcp_ipv4_socket();
    address = create_ipv4_address("127.0.0.1", 8080);

    connect_to_server(socketFD, address);

    int message_count;
    char **messages = read_and_parse_json_from_file("messages.json", &message_count);

    if (messages)
    {
        pthread_t id;
        pthread_create(&id, NULL, process_packets, NULL);

        for (int i = 0; i < message_count; i++)
        {
            packet *packet = new_packet(PKT_PRODUCER_PUBLISH, messages[i]);

            current_id = packet->id;
            ack_received = false;

            int retries = 0;
            while (retries < MAX_RETRIES)
            {
                printf("Sending packet: ");
                print_uid(&current_id);
                print_packet(packet);

                send_packet(socketFD, packet);

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

            usleep(100000); // 100ms
        }

        log_info("Sent %d packets, received %d acks", message_count, ack_count);

        free(messages);
        pthread_join(id, NULL);
    }

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
            case PKT_BAD_FORMAT:
                log_error("[process_packets(void*)] : Bad format packet received");
                break;
            case PKT_INCOMPLETE:
                log_error("[process_packets(void*)] : Incomplete packet received");
                break;

            case PKT_UNKNOWN:
            default:
                log_info("[process_packets(void*)] : Packet unknown");
            }
        }
    }

    return NULL;
}
