#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include "client_type.h"
#include "packets.h"

void send_simple_packet(int socket_num, int seq_number, int flag) {
    struct normal_header header;
    header.seq_number = htonl(seq_number);
    header.flag = flag;
    
    char packet[HEADER_LENGTH];
    memcpy(packet, &header, HEADER_LENGTH);
    send(socket_num, packet, sizeof(packet), 0);
}

void handle_message_packet(char *packet) {
    uint8_t dst_handle_length = *(packet + HEADER_LENGTH);
    char *dst_handle = (packet + HEADER_LENGTH + HANDLE_LENGTH);
    uint8_t src_handle_length = *(packet + HEADER_LENGTH + HANDLE_LENGTH
                                  + dst_handle_length);
    char *src_handle = (packet + HEADER_LENGTH + HANDLE_LENGTH
                        + dst_handle_length + HANDLE_LENGTH);
    char *message = (packet + HEADER_LENGTH + HANDLE_LENGTH + dst_handle_length
                     + HANDLE_LENGTH + src_handle_length);
    
    printf("From %s to %s: %s\n", src_handle, dst_handle, message);
}

int check_handle(int socket_num, char *packet, struct clients_t *clients) {
    uint8_t handle_length = *(packet + HEADER_LENGTH);
    char *handle = (packet + HEADER_LENGTH + HANDLE_LENGTH);
    struct client_t *temp = clients->client;
    
    while (temp != NULL) {
        if (strncmp(handle, temp->handle, handle_length) == 0) {
            return SERVER_REJECT_HANDLE;
        }
        temp = temp->next;
    }
    
    add_handle(&clients, socket_num, handle, handle_length);
    return SERVER_ACCEPT_HANDLE;
}

void handle_packet(int socket_num, struct clients_t *clients, char *packet) {
    struct normal_header header;
    memcpy(&header, packet, HEADER_LENGTH);
    
    if (header.flag == CLIENT_INITIAL_PACKET) {
        if (check_handle(socket_num, packet, clients) == SERVER_ACCEPT_HANDLE) {
            send_simple_packet(socket_num, 0, SERVER_ACCEPT_HANDLE);
        } else {
            send_simple_packet(socket_num, 0, SERVER_REJECT_HANDLE);
        }
    } else if (header.flag == CLIENT_BROADCAST) {
        printf("Broadcast received\n");
    } else if (header.flag == CLIENT_MESSAGE) {
        handle_message_packet(packet);
    }
}

void handle_client(int client_socket_num, struct clients_t *clients) {
    char packet[BUFF_SIZE];

    int recv_result = recv(client_socket_num, packet, BUFF_SIZE, 0);
    if (recv_result < 0) {
        perror("Couldn't retrieve the message.");
    } else if (recv_result == 0) {
        printf("Client disconnected\n");
        remove_client(&clients, client_socket_num);
        close(client_socket_num);
    } else {
        handle_packet(client_socket_num, clients, packet);
    }
}

int wait_for_client(int server_socket_num, struct clients_t *clients) {
    fd_set fd_read;
    FD_ZERO(&fd_read);
    FD_SET(server_socket_num, &fd_read);
    
    int max = server_socket_num;
    for (int i=0; clients != NULL && i<clients->num_clients; i++) {
        FD_SET(get_client(clients, i), &fd_read);
        max = (get_client(clients, i) > max) ? get_client(clients, i) : max;
    }
    
    select(max+1, (fd_set*) &fd_read, (fd_set*) 0, (fd_set*) 0, NULL);
    
    if (FD_ISSET(server_socket_num, &fd_read)){
        return server_socket_num;
    }

    for (int i=0; clients != NULL && i<clients->num_clients; i++) {
        int client_socket = get_client(clients, i);
        if (FD_ISSET(client_socket, &fd_read)) {
            return client_socket;
        }
    }
    
    return -1;
}

void watch_for_clients(int socket_num) {
    struct clients_t *clients = NULL;
    
    if (listen(socket_num, 5) < 0) {
        perror("Couldn't listen for connections.");
        exit(-1);
    }
    
    while (1) {
        int socket_ready = wait_for_client(socket_num, clients);
        if (socket_ready == socket_num) {
            int client_socket_num;
            if ((client_socket_num = accept(socket_num, (struct sockaddr*) 0, (socklen_t*) 0)) < 0) {
                perror("Couldn't accept client connection.");
                exit(-1);
            }
            
            add_client(&clients, client_socket_num);
            printf("client connected\n");
        } else {
            handle_client(socket_ready, clients);
        }
    }
}


void setup_server(int socket_num, int port_number) {
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_number);
    
    if (bind(socket_num, (struct sockaddr*) &local_addr, sizeof(local_addr)) < 0) {
        perror("Couldn't setup server port.");
        exit(-1);
    }
    
    if (getsockname(socket_num, (struct sockaddr*) &local_addr, &local_len) < 0) {
        perror("Couldn't retrieve server name.");
        exit(-1);
    }
    
    printf("Server is using port %d \n", ntohs(local_addr.sin_port));
}

int get_server_socket() {
    int socket_num;
    
    if((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Couldn't create socket.");
        exit(-1);
    }
    
    return socket_num;
}

int main(int argc, char **argv) {
    int port_number = 0;
    
    if (argc > 2) {
        printf("\nUsage: server <optional: port number>\n\n");
        exit(-1);
    } else if (argc == 2) {
        port_number = atoi(argv[1]);
    }
    
    int socket_num = get_server_socket();
    setup_server(socket_num, port_number);
    
    watch_for_clients(socket_num);
    
    close(socket_num);
}